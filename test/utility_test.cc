#include <string>

#include "gtest/gtest.h"

#include "common/network/utility.h"
#include "test/test_common/environment.h"
#include "test/test_common/utility.h"

#include "nighthawk/source/common/utility.h"

namespace Nighthawk {

class UtilityTest : public testing::Test {
public:
  UtilityTest() {}
  void checkUriParsing(const std::string& uri_to_test, const std::string& host_and_port,
                       const std::string& host_without_port, const uint64_t port,
                       const std::string& scheme, const std::string& path) {
    const Uri uri = Uri::Parse(uri_to_test);
    EXPECT_EQ(host_and_port, uri.host_and_port());
    EXPECT_EQ(host_without_port, uri.host_without_port());
    EXPECT_EQ(port, uri.port());
    EXPECT_EQ(scheme, uri.scheme());
    EXPECT_EQ(path, uri.path());
  }

  int32_t getCpuCountFromSet(cpu_set_t& set) { return CPU_COUNT(&set); }
};

TEST_F(UtilityTest, PerfectlyFineUrl) {
  checkUriParsing("http://a/b", "a:80", "a", 80, "http", "/b");
}

TEST_F(UtilityTest, Defaults) {
  checkUriParsing("a", "a:80", "a", 80, "http", "/");
  checkUriParsing("a/", "a:80", "a", 80, "http", "/");
  checkUriParsing("https://a", "a:443", "a", 443, "https", "/");
}

TEST_F(UtilityTest, SchemeIsLowerCased) {
  const Uri uri = Uri::Parse("HTTP://a");
  EXPECT_EQ("http", uri.scheme());
}

TEST_F(UtilityTest, ExplicitPort) {
  const Uri u1 = Uri::Parse("HTTP://a:111");
  EXPECT_EQ(111, u1.port());

  EXPECT_THROW(Uri::Parse("HTTP://a:-111"), UriException);
  EXPECT_THROW(Uri::Parse("HTTP://a:0"), UriException);
}

TEST_F(UtilityTest, SchemeWeDontUnderstand) { EXPECT_THROW(Uri::Parse("foo://a"), UriException); }

TEST_F(UtilityTest, Empty) { EXPECT_THROW(Uri::Parse(""), UriException); }

TEST_F(UtilityTest, HostStartsWithMinus) { EXPECT_THROW(Uri::Parse("http://-a"), UriException); }

TEST_F(UtilityTest, Ipv6Address) {
  const Uri u = Uri::Parse("http://[::1]:81/bar");
  EXPECT_EQ("[::1]", u.host_without_port());
  EXPECT_EQ("[::1]:81", u.host_and_port());
  EXPECT_EQ(81, u.port());

  const Uri u2 = Uri::Parse("http://[::1]/bar");
  EXPECT_EQ("[::1]", u2.host_without_port());
  EXPECT_EQ("[::1]:80", u2.host_and_port());
  EXPECT_EQ(80, u2.port());
}

TEST_F(UtilityTest, FindPortSeparator) {
  EXPECT_EQ(absl::string_view::npos, Uri::findPortSeparator("127.0.0.1"));
  EXPECT_EQ(5, Uri::findPortSeparator("[::1]:80"));
  EXPECT_EQ(absl::string_view::npos, Uri::findPortSeparator("[::1]"));
  EXPECT_EQ(9, Uri::findPortSeparator("127.0.0.1:80"));
  EXPECT_EQ(absl::string_view::npos, Uri::findPortSeparator("127.0.0.1"));

  EXPECT_EQ(absl::string_view::npos, Uri::findPortSeparator("foo.com"));

  EXPECT_EQ(7, Uri::findPortSeparator("foo.com:80"));
  EXPECT_EQ(8, Uri::findPortSeparator("8foo.com:80"));
}

class UtilityAddressResolutionTest
    : public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  Envoy::Network::Address::InstanceConstSharedPtr
  testResolution(absl::string_view uri, Envoy::Network::DnsLookupFamily address_family) {
    Envoy::Api::ApiPtr api = Envoy::Api::createApiForTest();
    auto dispatcher = api->allocateDispatcher();
    auto u = Uri::Parse(uri);
    return u.resolve(*dispatcher, address_family);
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, UtilityAddressResolutionTest,
                         testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(UtilityAddressResolutionTest, AddressResolution) {
  if (GetParam() == Envoy::Network::Address::IpVersion::v4) {
    Envoy::Network::DnsLookupFamily address_family = Envoy::Network::DnsLookupFamily::V4Only;
    EXPECT_EQ("127.0.0.1:80", testResolution("127.0.0.1", address_family)->asString());
    EXPECT_EQ("127.0.0.1:81", testResolution("127.0.0.1:81", address_family)->asString());
    EXPECT_EQ("127.0.0.1:80", testResolution("localhost", address_family)->asString());
    EXPECT_EQ("127.0.0.1:81", testResolution("localhost:81", address_family)->asString());
    EXPECT_THROW(testResolution("[::1]", address_family), UriException);
    EXPECT_THROW(testResolution("::1:81", address_family), UriException);
  } else {
    Envoy::Network::DnsLookupFamily address_family = Envoy::Network::DnsLookupFamily::V6Only;
    EXPECT_EQ("[::1]:80", testResolution("localhost", address_family)->asString());
    EXPECT_EQ("[::1]:81", testResolution("localhost:81", address_family)->asString());
    EXPECT_EQ("[::1]:80", testResolution("[::1]", address_family)->asString());
    EXPECT_EQ("[::1]:81", testResolution("::1:81", address_family)->asString());
    EXPECT_THROW(testResolution("127.0.0.1", address_family), UriException);
    EXPECT_THROW(testResolution("127.0.0.1:80", address_family), UriException);
  }
}

TEST_P(UtilityAddressResolutionTest, AddressResolutionBadAddresses) {
  Envoy::Network::DnsLookupFamily address_family = Envoy::Network::DnsLookupFamily::Auto;

  EXPECT_THROW(testResolution("bad#host", address_family), UriException);
  EXPECT_THROW(testResolution("-foo.com", address_family), UriException);
  EXPECT_THROW(testResolution("[foo.com", address_family), UriException);
  EXPECT_THROW(testResolution("foo]", address_family), UriException);
  EXPECT_THROW(testResolution(".", address_family), UriException);
  EXPECT_THROW(testResolution("..", address_family), UriException);
  EXPECT_THROW(testResolution("a..b", address_family), UriException);
}

TEST_P(UtilityAddressResolutionTest, ResolveTwiceReturnsCached) {
  Envoy::Network::DnsLookupFamily address_family =
      GetParam() == Envoy::Network::Address::IpVersion::v6
          ? Envoy::Network::DnsLookupFamily::V6Only
          : Envoy::Network::DnsLookupFamily::V4Only;

  Envoy::Api::ApiPtr api = Envoy::Api::createApiForTest();
  auto dispatcher = api->allocateDispatcher();
  auto u = Uri::Parse("localhost");

  EXPECT_EQ(u.resolve(*dispatcher, address_family).get(),
            u.resolve(*dispatcher, address_family).get());
}

// TODO(oschaaf): we probably want to move this out to another file.
TEST_F(UtilityTest, CpusWithAffinity) {
  cpu_set_t original_set;
  CPU_ZERO(&original_set);
  EXPECT_EQ(0, sched_getaffinity(0, sizeof(original_set), &original_set));

  uint32_t original_cpu_count = PlatformUtils::determineCpuCoresWithAffinity();
  EXPECT_EQ(original_cpu_count, getCpuCountFromSet(original_set));

  // Now the test, we set affinity to just the first cpu. We expect that to be reflected.
  // This will be a no-op on a single core system.
  cpu_set_t test_set;
  CPU_ZERO(&test_set);
  CPU_SET(0, &test_set);
  EXPECT_EQ(0, sched_setaffinity(0, sizeof(test_set), &test_set));
  EXPECT_EQ(1, PlatformUtils::determineCpuCoresWithAffinity());

  // Restore affinity to what it was.
  EXPECT_EQ(0, sched_setaffinity(0, sizeof(original_set), &original_set));
  EXPECT_EQ(original_cpu_count, PlatformUtils::determineCpuCoresWithAffinity());
}

} // namespace Nighthawk
