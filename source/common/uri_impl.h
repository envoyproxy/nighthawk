#pragma once

#include <string>

#include "absl/strings/string_view.h"

#include "envoy/stats/store.h"

#include "common/common/logger.h"
#include "common/network/dns_impl.h"
#include "common/network/utility.h"

#include "nighthawk/common/exception.h"
#include "nighthawk/common/uri.h"

namespace Nighthawk {

class UriImpl : public Uri, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  UriImpl(absl::string_view uri);
  const std::string& host_and_port() const override { return host_and_port_; }
  const std::string& host_without_port() const override { return host_without_port_; }
  const std::string& path() const override { return path_; }
  uint64_t port() const override { return port_; }
  const std::string& scheme() const override { return scheme_; }
  Envoy::Network::Address::InstanceConstSharedPtr
  resolve(Envoy::Event::Dispatcher& dispatcher,
          const Envoy::Network::DnsLookupFamily dns_lookup_family) override;
  Envoy::Network::Address::InstanceConstSharedPtr address() const override {
    ASSERT(resolve_attempted_, "resolve() must be called first.");
    return address_;
  }

private:
  bool isValid() const;
  bool performDnsLookup(Envoy::Event::Dispatcher& dispatcher,
                        const Envoy::Network::DnsLookupFamily dns_lookup_family);

  // TODO(oschaaf): username, password, query etc. But we may want to look at
  // pulling in a mature uri parser.
  std::string host_and_port_;
  std::string host_without_port_;
  std::string path_;
  uint64_t port_{};
  std::string scheme_;

  Envoy::Network::Address::InstanceConstSharedPtr address_;
  bool resolve_attempted_{};
};

} // namespace Nighthawk