
#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

#include "common/http/utility.h"
#include "common/network/utility.h"

#include "common/utility.h"

namespace Nighthawk {

namespace PlatformUtils {

// returns 0 on failure. returns the number of HW CPU's
// that the current thread has affinity with.
// TODO(oschaaf): mull over what to do w/regard to hyperthreading.
uint32_t determineCpuCoresWithAffinity() {
  const pthread_t thread = pthread_self();
  cpu_set_t cpuset;
  int i;

  CPU_ZERO(&cpuset);
  i = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (i == 0) {
    return CPU_COUNT(&cpuset);
  }
  return 0;
}

} // namespace PlatformUtils

std::map<std::string, uint64_t>
Utility::mapCountersFromStore(const Envoy::Stats::Store& store,
                              const StoreCounterFilter& filter) const {
  std::map<std::string, uint64_t> results;

  for (const auto& stat : store.counters()) {
    if (filter(stat->name(), stat->value())) {
      results[stat->name()] = stat->value();
    }
  }

  return results;
}

Uri Uri::Parse(absl::string_view uri) {
  auto r = Uri(uri);
  if (!r.isValid()) {
    throw UriException("Invalid URI");
  }
  return r;
}

bool Uri::isValid() const {
  return (scheme_ == "http" || scheme_ == "https") && (port_ > 0 && port_ <= 65535) &&
         // We check that we do not start with '-' because that overlaps with CLI argument
         // parsing. For other hostname validation, we defer to parseInternetAddressAndPort() and
         // dns resolution later on.
         host_without_port_.size() > 0 && host_without_port_[0] != '-';
}

size_t Uri::findPortSeparator(absl::string_view hostname) {
  if (hostname.size() > 0 && hostname[0] == '[') {
    return hostname.find(":", hostname.find(']'));
  }
  return hostname.rfind(":");
}

Uri::Uri(absl::string_view uri) : scheme_("http") {
  absl::string_view host, path;
  Envoy::Http::Utility::extractHostPathFromUri(uri, host, path);

  if (host.empty()) {
    throw UriException("Invalid URI (no host)");
  }

  host_and_port_ = std::string(host);
  path_ = std::string(path);
  const bool is_https = absl::StartsWith(uri, "https://");
  const size_t scheme_end = uri.find("://", 0);
  if (scheme_end != std::string::npos) {
    scheme_ = absl::AsciiStrToLower(uri.substr(0, scheme_end));
  }

  const size_t colon_index = findPortSeparator(host_and_port_);

  if (colon_index == absl::string_view::npos) {
    port_ = is_https ? 443 : 80;
    host_without_port_ = host_and_port_;
    host_and_port_ = fmt::format("{}:{}", host_and_port_, port_);
  } else {
    port_ = std::stoi(host_and_port_.substr(colon_index + 1));
    host_without_port_ = host_and_port_.substr(0, colon_index);
  }
}

bool Uri::performDnsLookup(Envoy::Event::Dispatcher& dispatcher,
                           const Envoy::Network::DnsLookupFamily dns_lookup_family) {
  auto dns_resolver = dispatcher.createDnsResolver({});
  auto hostname = host_without_port();

  if (!hostname.empty() && hostname[0] == '[' && hostname[hostname.size() - 1] == ']') {
    hostname = absl::StrReplaceAll(hostname, {{"[", ""}, {"]", ""}});
  }

  Envoy::Network::ActiveDnsQuery* active_dns_query_ = dns_resolver->resolve(
      hostname, dns_lookup_family,
      [this, &dispatcher, &active_dns_query_](
          const std::list<Envoy::Network::Address::InstanceConstSharedPtr>&& address_list) -> void {
        active_dns_query_ = nullptr;
        if (!address_list.empty()) {
          address_ = Envoy::Network::Utility::getAddressWithPort(*address_list.front(), port());
          ENVOY_LOG(debug, "DNS resolution complete for {} ({} entries, using {}).",
                    host_without_port(), address_list.size(), address_->asString());
        }
        dispatcher.exit();
      });

  // Wait for DNS resolution to complete before proceeding.
  dispatcher.run(Envoy::Event::Dispatcher::RunType::Block);
  return address_ != nullptr;
}

Envoy::Network::Address::InstanceConstSharedPtr
Uri::resolve(Envoy::Event::Dispatcher& dispatcher,
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
    ENVOY_LOG(warn, "Could not resolve '{}'", host_without_port());
    address_.reset();
    throw UriException("Could not determine address");
  }
  return address_;
}

} // namespace Nighthawk