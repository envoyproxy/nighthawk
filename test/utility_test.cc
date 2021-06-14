#include <string>

#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/test_common/utility.h"

#include "source/common/uri_impl.h"
#include "source/common/utility.h"

#include "test/test_common/environment.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

class UtilityTest : public Test {
public:
  UtilityTest() = default;
  void checkUriParsing(absl::string_view uri_to_test, absl::string_view hostAndPort,
                       absl::string_view hostWithoutPort, const uint64_t port,
                       absl::string_view scheme, absl::string_view path,
                       absl::string_view uri_default_protocol = "") {
    const UriImpl uri = uri_default_protocol == "" ? UriImpl(uri_to_test)
                                                   : UriImpl(uri_to_test, uri_default_protocol);
    EXPECT_EQ(hostAndPort, uri.hostAndPort());
    EXPECT_EQ(hostWithoutPort, uri.hostWithoutPort());
    EXPECT_EQ(port, uri.port());
    EXPECT_EQ(scheme, uri.scheme());
    EXPECT_EQ(path, uri.path());
  }
};

TEST_F(UtilityTest, PerfectlyFineUrl) {
  checkUriParsing("http://a/b", "a:80", "a", 80, "http", "/b");
}

TEST_F(UtilityTest, Defaults) {
  checkUriParsing("a", "a:80", "a", 80, "http", "/");
  checkUriParsing("a/", "a:80", "a", 80, "http", "/");
  checkUriParsing("https://a", "a:443", "a", 443, "https", "/");
  checkUriParsing("grpc://a", "a:8443", "a", 8443, "grpc", "/");
  checkUriParsing("a", "a:8443", "a", 8443, "grpc", "/", "grpc");
}

TEST_F(UtilityTest, SchemeIsLowerCased) {
  const UriImpl uri = UriImpl("HTTP://a");
  EXPECT_EQ("http", uri.scheme());
}

TEST_F(UtilityTest, ExplicitPort) {
  const UriImpl u1 = UriImpl("HTTP://a:111");
  EXPECT_EQ(111, u1.port());

  EXPECT_THROW(UriImpl("HTTP://a:-111"), UriException);
  EXPECT_THROW(UriImpl("HTTP://a:0"), UriException);
}

TEST_F(UtilityTest, SchemeWeDontUnderstand) { EXPECT_THROW(UriImpl("foo://a"), UriException); }

TEST_F(UtilityTest, Empty) { EXPECT_THROW(UriImpl(""), UriException); }

TEST_F(UtilityTest, HostStartsWithMinus) { EXPECT_THROW(UriImpl("http://-a"), UriException); }

TEST_F(UtilityTest, Ipv6Address) {
  const UriImpl u = UriImpl("http://[::1]:81/bar");
  EXPECT_EQ("[::1]", u.hostWithoutPort());
  EXPECT_EQ("[::1]:81", u.hostAndPort());
  EXPECT_EQ(81, u.port());

  const UriImpl u2 = UriImpl("http://[::1]/bar");
  EXPECT_EQ("[::1]", u2.hostWithoutPort());
  EXPECT_EQ("[::1]:80", u2.hostAndPort());
  EXPECT_EQ(80, u2.port());
}

TEST_F(UtilityTest, FindPortSeparator) {
  EXPECT_EQ(absl::string_view::npos, Utility::findPortSeparator("127.0.0.1"));
  EXPECT_EQ(5, Utility::findPortSeparator("[::1]:80"));
  EXPECT_EQ(absl::string_view::npos, Utility::findPortSeparator("[::1]"));
  EXPECT_EQ(9, Utility::findPortSeparator("127.0.0.1:80"));
  EXPECT_EQ(absl::string_view::npos, Utility::findPortSeparator("127.0.0.1"));

  EXPECT_EQ(absl::string_view::npos, Utility::findPortSeparator("foo.com"));

  EXPECT_EQ(7, Utility::findPortSeparator("foo.com:80"));
  EXPECT_EQ(8, Utility::findPortSeparator("8foo.com:80"));
}

class UtilityAddressResolutionTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  Envoy::Network::Address::InstanceConstSharedPtr
  testResolution(absl::string_view uri, Envoy::Network::DnsLookupFamily address_family) {
    Envoy::Api::ApiPtr api = Envoy::Api::createApiForTest();
    auto dispatcher = api->allocateDispatcher("uri_resolution_thread");
    auto u = UriImpl(uri);
    return u.resolve(*dispatcher, address_family);
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, UtilityAddressResolutionTest,
                         ValuesIn(TestEnvironment::getIpVersionsForTest()),
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
  auto dispatcher = api->allocateDispatcher("test_thread");
  auto u = UriImpl("localhost");

  EXPECT_EQ(u.resolve(*dispatcher, address_family).get(),
            u.resolve(*dispatcher, address_family).get());
}

TEST_F(UtilityTest, TranslateAddressFamilyGoodValues) {
  EXPECT_EQ(Envoy::Network::DnsLookupFamily::V6Only,
            Utility::translateFamilyOptionString(
                nighthawk::client::AddressFamily_AddressFamilyOptions_V6));
  EXPECT_EQ(Envoy::Network::DnsLookupFamily::V4Only,
            Utility::translateFamilyOptionString(
                nighthawk::client::AddressFamily_AddressFamilyOptions_V4));
  EXPECT_EQ(Envoy::Network::DnsLookupFamily::Auto,
            Utility::translateFamilyOptionString(
                nighthawk::client::AddressFamily_AddressFamilyOptions_AUTO));
}

TEST_F(UtilityTest, MapCountersFromStore) {
  Envoy::Stats::IsolatedStoreImpl store;
  store.counterFromString("foo").inc();
  store.counterFromString("worker.2.bar").inc();
  store.counterFromString("worker.1.bar").inc();
  uint64_t filter_delegate_hit_count = 0;
  const auto& counters = Utility().mapCountersFromStore(
      store, [&filter_delegate_hit_count](absl::string_view name, uint64_t value) {
        filter_delegate_hit_count++;
        return value == 1 && (name == "worker.2.bar" || name == "worker.1.bar");
      });
  EXPECT_EQ(filter_delegate_hit_count, 3);
  ASSERT_EQ(counters.size(), 1);
  EXPECT_EQ(counters.begin()->second, 2);
}

TEST_F(UtilityTest, MultipleSemicolons) {
  EXPECT_THROW(UriImpl("HTTP://HTTP://a:111"), UriException);
}

} // namespace Nighthawk
