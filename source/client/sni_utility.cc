#include "source/client/sni_utility.h"

#include "absl/strings/strip.h"

namespace Nighthawk {
namespace Client {

std::string SniUtility::computeSniHost(const std::vector<UriPtr>& uris,
                                       const std::vector<std::string>& request_headers,
                                       const Envoy::Http::Protocol protocol) {
  std::string uri_sni;
  std::string request_sni;
  bool ambiguous = false;
  const bool consider_authority_header =
      protocol == Envoy::Http::Protocol::Http2 || protocol == Envoy::Http::Protocol::Http3;
  // If we only have a single target uri, we set ourselves up for sni based on the
  // host from the uri.
  if (uris.size() == 1) {
    uri_sni = std::string(uris[0]->hostWithoutPort());
  }

  // A Host: request-header overrides what we came up with above. Notably this also applies
  // when multiple target uris are involved.
  for (const std::string& header : request_headers) {
    const std::string lowered_header = absl::AsciiStrToLower(header);
    if (absl::StartsWithIgnoreCase(lowered_header, "host:") ||
        (absl::StartsWithIgnoreCase(lowered_header, ":authority:") && consider_authority_header)) {
      ambiguous = ambiguous || !request_sni.empty();
      absl::string_view host = absl::StripPrefix(lowered_header, "host:");
      host = absl::StripPrefix(host, ":authority:");
      host = absl::StripAsciiWhitespace(host);
      request_sni = std::string(host);
    }
  }
  std::string sni_host;
  if (ambiguous) {
    ENVOY_LOG(warn, "Ambiguous host request headers detected");
  } else {
    sni_host = request_sni.empty() ? uri_sni : request_sni;
  }
  ENVOY_LOG(debug, "computed server name indication: '{}'", sni_host);
  return sni_host;
}

} // namespace Client
} // namespace Nighthawk