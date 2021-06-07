#include "source/common/uri_impl.h"

#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy_api/envoy/config/core/v3/resolver.pb.h"

#include "source/common/utility.h"

#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace Nighthawk {

bool UriImpl::isValid() const {
  return (scheme_ == "http" || scheme_ == "https" || scheme_ == "zipkin" || scheme_ == "grpc") &&
         (port_ > 0 && port_ <= 65535) &&
         // We check that we do not start with '-' because that overlaps with CLI argument
         // parsing. For other hostname validation, we defer to parseInternetAddressAndPort() and
         // dns resolution later on.
         !host_without_port_.empty() && host_without_port_[0] != '-';
}

UriImpl::UriImpl(absl::string_view uri, absl::string_view default_scheme)
    : scheme_(default_scheme) {
  absl::string_view host, path;
  Envoy::Http::Utility::extractHostPathFromUri(uri, host, path);

  if (host.empty()) {
    throw UriException("Invalid URI (no host)");
  }

  host_and_port_ = std::string(host);
  path_ = std::string(path);
  const size_t scheme_end = uri.find("://", 0);
  if (scheme_end != std::string::npos) {
    scheme_ = absl::AsciiStrToLower(uri.substr(0, scheme_end));
  }
  uint32_t default_port = 80;
  if (scheme_ == "https") {
    default_port = 443;
  } else if (scheme_ == "grpc") {
    default_port = 8443;
  }

  const size_t colon_index = Utility::findPortSeparator(host_and_port_);

  if (colon_index == absl::string_view::npos) {
    port_ = default_port;
    host_without_port_ = host_and_port_;
    host_and_port_ = fmt::format("{}:{}", host_and_port_, port_);
  } else {
    if (absl::SimpleAtoi(host_and_port_.substr(colon_index + 1), &port_)) {
      host_without_port_ = host_and_port_.substr(0, colon_index);
    } else {
      throw UriException("Invalid URI, couldn't parse port");
    }
  }
  if (!isValid()) {
    throw UriException("Invalid URI");
  }
}

bool UriImpl::performDnsLookup(Envoy::Event::Dispatcher& dispatcher,
                               const Envoy::Network::DnsLookupFamily dns_lookup_family) {
  envoy::config::core::v3::DnsResolverOptions dns_resolver_options;
  auto dns_resolver = dispatcher.createDnsResolver({}, dns_resolver_options);
  std::string hostname = std::string(hostWithoutPort());

  if (!hostname.empty() && hostname[0] == '[' && hostname[hostname.size() - 1] == ']') {
    hostname = absl::StrReplaceAll(hostname, {{"[", ""}, {"]", ""}});
  }

  Envoy::Network::ActiveDnsQuery* active_dns_query_ = dns_resolver->resolve(
      hostname, dns_lookup_family,
      [this, &dispatcher,
       &active_dns_query_](Envoy::Network::DnsResolver::ResolutionStatus status,
                           std::list<Envoy::Network::DnsResponse>&& response) -> void {
        active_dns_query_ = nullptr;
        if (!response.empty() && status == Envoy::Network::DnsResolver::ResolutionStatus::Success) {
          address_ =
              Envoy::Network::Utility::getAddressWithPort(*response.front().address_, port());
          ENVOY_LOG(debug, "DNS resolution complete for {} ({} entries, using {}).",
                    hostWithoutPort(), response.size(), address_->asString());
        }
        dispatcher.exit();
      });

  // Wait for DNS resolution to complete before proceeding.
  dispatcher.run(Envoy::Event::Dispatcher::RunType::Block);
  return address_ != nullptr;
}

Envoy::Network::Address::InstanceConstSharedPtr
UriImpl::resolve(Envoy::Event::Dispatcher& dispatcher,
                 const Envoy::Network::DnsLookupFamily dns_lookup_family) {
  if (resolve_attempted_) {
    return address_;
  }
  resolve_attempted_ = true;

  bool ok = performDnsLookup(dispatcher, dns_lookup_family);

  // Ensure that we figured out a fitting match for the requested dns lookup family.
  ok = ok && !((dns_lookup_family == Envoy::Network::DnsLookupFamily::V6Only &&
                address_->ip()->ipv6() == nullptr) ||
               (dns_lookup_family == Envoy::Network::DnsLookupFamily::V4Only &&
                address_->ip()->ipv4() == nullptr));
  if (!ok) {
    ENVOY_LOG(warn, "Could not resolve '{}'", hostWithoutPort());
    address_.reset();
    throw UriException("Could not determine address");
  }
  return address_;
}

} // namespace Nighthawk
