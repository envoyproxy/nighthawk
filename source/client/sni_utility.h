#pragma once

#include <string>
#include <vector>

#include "nighthawk/common/uri.h"

#include "external/envoy//envoy/http/protocol.h"

namespace Nighthawk {
namespace Client {

class SniUtility : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * Computes the SNI host based on the passed in uri(s), request headers, and protocol.
   * Given a vector of Uris containing a single entry, its host fragment will be considered.
   * Subsequently the passed request headers will be scanned for any host headers to override any
   * Uri-derived sni host. If the passed in protocol is HTTP/2 or higher, next to host, :authority
   * will be considered as well.
   *
   * @param parsed_uris The parsed target uris configured for the load test.
   * @param request_headers Request headers to scan.
   * @param protocol The anticipated protocol that will be used.
   * @return std::string The sni-host derived from the configured load test target Uris and any
   * host/authority request-headers found. Empty if no (unambiguous) sni host could be derived.
   */
  static std::string computeSniHost(const std::vector<UriPtr>& parsed_uris,
                                    const std::vector<std::string>& request_headers,
                                    const Envoy::Http::Protocol protocol);
};

} // namespace Client
} // namespace Nighthawk