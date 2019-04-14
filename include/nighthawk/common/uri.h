#pragma once

#include <string>

#include "absl/strings/string_view.h"

#include "common/network/dns_impl.h"
#include "common/network/utility.h"

#include "nighthawk/common/exception.h"

namespace Nighthawk {

class UriException : public NighthawkException {
public:
  UriException(const std::string& message) : NighthawkException(message) {}
};

class Uri {
public:
  virtual ~Uri() = default;
  virtual absl::string_view host_and_port() const PURE;
  virtual absl::string_view host_without_port() const PURE;
  virtual absl::string_view path() const PURE;
  virtual uint64_t port() const PURE;
  virtual absl::string_view scheme() const PURE;

  virtual Envoy::Network::Address::InstanceConstSharedPtr
  resolve(Envoy::Event::Dispatcher& dispatcher,
          const Envoy::Network::DnsLookupFamily dns_lookup_family) PURE;
  virtual Envoy::Network::Address::InstanceConstSharedPtr address() const PURE;
};

using UriPtr = std::unique_ptr<Uri>;

} // namespace Nighthawk