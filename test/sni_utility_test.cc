#include "external/envoy/test/test_common/utility.h"

#include "common/uri_impl.h"

#include "client/sni_utility.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

class SniUtilityTest : public Test {
public:
  void checkSniHostComputation(const std::vector<std::string>& uris,
                               const std::vector<std::string>& request_headers,
                               const Envoy::Http::Protocol protocol,
                               absl::string_view expected_sni_host) {
    std::vector<UriPtr> parsed_uris;
    parsed_uris.reserve(uris.size());
    for (const std::string& uri : uris) {
      parsed_uris.push_back(std::make_unique<UriImpl>(uri));
    }
    std::string sni_host = SniUtility::computeSniHost(parsed_uris, request_headers, protocol);
    EXPECT_EQ(sni_host, expected_sni_host);
  }
};

TEST_F(SniUtilityTest, SniHostComputation) {
  const std::vector<Envoy::Http::Protocol> all_protocols{
      Envoy::Http::Protocol::Http10, Envoy::Http::Protocol::Http11, Envoy::Http::Protocol::Http2,
      Envoy::Http::Protocol::Http3};
  for (const auto protocol : all_protocols) {
    checkSniHostComputation({"localhost"}, {}, protocol, "localhost");
    checkSniHostComputation({"localhost:81"}, {}, protocol, "localhost");
    checkSniHostComputation({"localhost"}, {"Host: foo"}, protocol, "foo");
    checkSniHostComputation({"localhost:81"}, {"Host: foo"}, protocol, "foo");
    const std::string expected_sni_host(protocol >= Envoy::Http::Protocol::Http2 ? "foo"
                                                                                 : "localhost");
    checkSniHostComputation({"localhost"}, {":authority: foo"}, protocol, expected_sni_host);
    checkSniHostComputation({"localhost:81"}, {":authority: foo"}, protocol, expected_sni_host);
  }
}

} // namespace Client
} // namespace Nighthawk
