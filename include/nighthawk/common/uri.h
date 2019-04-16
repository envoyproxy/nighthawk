#pragma once

#include <string>

#include "absl/strings/string_view.h"

#include "common/network/dns_impl.h"
#include "common/network/utility.h"

#include "nighthawk/common/exception.h"

namespace Nighthawk {

/**
 * Any exception thrown by Uri shall inherit from UriException.
 */
class UriException : public NighthawkException {
public:
  UriException(const std::string& message) : NighthawkException(message) {}
};

/**
 * Abstract Uri interface.
 */
class Uri {
public:
  virtual ~Uri() = default;

  /**
   * @return absl::string_view containing the "host:port" fragment of the parsed uri. The port
   * will be explicitly set in case it is the default for the protocol. Do not use the returned
   * value after the Uri instance is destructed.
   */
  virtual absl::string_view hostAndPort() const PURE;

  /**
   * @return absl::string_view containing the "host" fragment parsed uri. Do not use the
   * returned value after the Uri instance is destructed.
   */
  virtual absl::string_view hostWithoutPort() const PURE;

  /**
   * @return absl::string_view containing the "/path" fragment of the parsed uri.
   */
  virtual absl::string_view path() const PURE;

  /**
   * @return uint64_t returns the port of the parsed uri.
   */
  virtual uint64_t port() const PURE;

  /**
   * @return absl::string_view returns the scheme of the parsed uri. Do not use the
   * returned value after the Uri instance is destructed.
   */
  virtual absl::string_view scheme() const PURE;

  /**
   * Synchronously resolves the parsed host from the uri to an ip-address.
   * @param dispatcher Dispatcher to use for resolving.
   * @param dns_lookup_family Allows specifying Ipv4, Ipv6, or Auto as the preferred returned
   * address family.
   * @return Envoy::Network::Address::InstanceConstSharedPtr the resolved address.
   */
  virtual Envoy::Network::Address::InstanceConstSharedPtr
  resolve(Envoy::Event::Dispatcher& dispatcher,
          const Envoy::Network::DnsLookupFamily dns_lookup_family) PURE;

  /**
   * @return Envoy::Network::Address::InstanceConstSharedPtr a cached copy of an earlier call to
   * resolve(), which must have been called successfully first.
   */
  virtual Envoy::Network::Address::InstanceConstSharedPtr address() const PURE;
};

using UriPtr = std::unique_ptr<Uri>;

} // namespace Nighthawk