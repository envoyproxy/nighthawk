#include "envoy/api/v2/core/base.pb.h"
#include "envoy/config/core/v3/base.pb.h"

#include "external/envoy/test/test_common/utility.h"

#include "api/server/response_options.pb.validate.h"

#include "source/server/configuration.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace Server {
namespace Configuration {
namespace {

using ::Envoy::Http::LowerCaseString;
using ::Envoy::Http::TestResponseHeaderMapImpl;

TEST(UpgradeDeprecatedEnvoyV2HeaderValueOptionToV3Test, UpgradesEmptyHeaderValue) {
  envoy::api::v2::core::HeaderValueOption v2_header_value_option;
  envoy::config::core::v3::HeaderValueOption v3_header_value_option =
      upgradeDeprecatedEnvoyV2HeaderValueOptionToV3(v2_header_value_option);

  EXPECT_FALSE(v3_header_value_option.has_append());
  EXPECT_FALSE(v3_header_value_option.has_header());
}

TEST(UpgradeDeprecatedEnvoyV2HeaderValueOptionToV3Test, UpgradesHeaderValueWithHeaderAndAppendSet) {
  envoy::api::v2::core::HeaderValueOption v2_header_value_option;
  v2_header_value_option.mutable_append()->set_value(true);
  v2_header_value_option.mutable_header()->set_key("key");
  v2_header_value_option.mutable_header()->set_value("value");

  envoy::config::core::v3::HeaderValueOption v3_header_value_option =
      upgradeDeprecatedEnvoyV2HeaderValueOptionToV3(v2_header_value_option);

  EXPECT_TRUE(v3_header_value_option.append().value());
  EXPECT_EQ(v3_header_value_option.header().key(), "key");
  EXPECT_EQ(v3_header_value_option.header().value(), "value");
}

TEST(UpgradeDeprecatedEnvoyV2HeaderValueOptionToV3Test, UpgradesHeaderValueWithHeaderOnly) {
  envoy::api::v2::core::HeaderValueOption v2_header_value_option;
  v2_header_value_option.mutable_header()->set_key("key");
  v2_header_value_option.mutable_header()->set_value("value");

  envoy::config::core::v3::HeaderValueOption v3_header_value_option =
      upgradeDeprecatedEnvoyV2HeaderValueOptionToV3(v2_header_value_option);

  EXPECT_FALSE(v3_header_value_option.has_append());
  EXPECT_EQ(v3_header_value_option.header().key(), "key");
  EXPECT_EQ(v3_header_value_option.header().value(), "value");
}

TEST(UpgradeDeprecatedEnvoyV2HeaderValueOptionToV3Test, UpgradesHeaderValueWithAppendOnly) {
  envoy::api::v2::core::HeaderValueOption v2_header_value_option;
  v2_header_value_option.mutable_append()->set_value(true);

  envoy::config::core::v3::HeaderValueOption v3_header_value_option =
      upgradeDeprecatedEnvoyV2HeaderValueOptionToV3(v2_header_value_option);

  EXPECT_TRUE(v3_header_value_option.append().value());
  EXPECT_FALSE(v3_header_value_option.has_header());
}

// Specifies the Envoy API version to use in the test configuration.
enum EnvoyApiVersion {
  EnvoyApiV2,
  EnvoyApiV3,
};

// Specifies if headers with duplicate key should be appended or replace the
// previous header.
enum HeaderAddMode {
  ReplaceOnDuplicateKey,
  AppendOnDuplicateKey,
};

// Creates a test configuration with three headers, two of which have the same
// key. The following headers are added:
//
//   key1: header1_value
//   key2: header2_value
//   key1: header3_value
//
// @param api_version determines the version of the Envoy API used in the
// created configuration.
// @param add_mode specifies how the header with the duplicate key is added.
// @return a configuration for the test.
nighthawk::server::ResponseOptions createTestConfiguration(EnvoyApiVersion api_version,
                                                           HeaderAddMode add_mode) {
  nighthawk::server::ResponseOptions configuration;

  if (api_version == EnvoyApiV2) {
    envoy::api::v2::core::HeaderValueOption* header1 = configuration.add_response_headers();
    header1->mutable_header()->set_key("key1");
    header1->mutable_header()->set_value("header1_value");

    envoy::api::v2::core::HeaderValueOption* header2 = configuration.add_response_headers();
    header2->mutable_header()->set_key("key2");
    header2->mutable_header()->set_value("header2_value");

    envoy::api::v2::core::HeaderValueOption* header3 = configuration.add_response_headers();
    header3->mutable_header()->set_key("key1");
    header3->mutable_header()->set_value("header3_value");
    if (add_mode == AppendOnDuplicateKey) {
      header3->mutable_append()->set_value("true");
    }
  } else if (api_version == EnvoyApiV3) {
    envoy::config::core::v3::HeaderValueOption* header1 = configuration.add_v3_response_headers();
    header1->mutable_header()->set_key("key1");
    header1->mutable_header()->set_value("header1_value");

    envoy::config::core::v3::HeaderValueOption* header2 = configuration.add_v3_response_headers();
    header2->mutable_header()->set_key("key2");
    header2->mutable_header()->set_value("header2_value");

    envoy::config::core::v3::HeaderValueOption* header3 = configuration.add_v3_response_headers();
    header3->mutable_header()->set_key("key1");
    header3->mutable_header()->set_value("header3_value");
    if (add_mode == AppendOnDuplicateKey) {
      header3->mutable_append()->set_value("true");
    }
  }
  return configuration;
}

// Creates the expected header map for the specified add mode.
//
// @param add_mode specifies how the header with the duplicate key is added.
// @return a header map populated with the expected headers.
TestResponseHeaderMapImpl createExpectedHeaderMap(HeaderAddMode add_mode) {
  TestResponseHeaderMapImpl expected_header_map;
  if (add_mode == ReplaceOnDuplicateKey) {
    expected_header_map.addCopy(LowerCaseString("key2"), "header2_value");
    expected_header_map.addCopy(LowerCaseString("key1"), "header3_value");
  } else if (add_mode == AppendOnDuplicateKey) {
    expected_header_map.addCopy(LowerCaseString("key1"), "header1_value");
    expected_header_map.addCopy(LowerCaseString("key2"), "header2_value");
    expected_header_map.addCopy(LowerCaseString("key1"), "header3_value");
  }
  return expected_header_map;
}

TEST(ApplyConfigToResponseHeaders, ReplacesHeadersFromEnvoyApiV2Config) {
  HeaderAddMode add_mode = ReplaceOnDuplicateKey;
  nighthawk::server::ResponseOptions configuration = createTestConfiguration(EnvoyApiV2, add_mode);

  TestResponseHeaderMapImpl header_map;
  applyConfigToResponseHeaders(header_map, configuration);
  TestResponseHeaderMapImpl expected_header_map = createExpectedHeaderMap(add_mode);

  EXPECT_EQ(header_map, expected_header_map) << "got header_map:\n"
                                             << header_map << "\nexpected_header_map:\n"
                                             << expected_header_map;
}

TEST(ApplyConfigToResponseHeaders, AppendsHeadersFromEnvoyApiV2Config) {
  HeaderAddMode add_mode = AppendOnDuplicateKey;
  nighthawk::server::ResponseOptions configuration = createTestConfiguration(EnvoyApiV2, add_mode);

  TestResponseHeaderMapImpl header_map;
  applyConfigToResponseHeaders(header_map, configuration);
  TestResponseHeaderMapImpl expected_header_map = createExpectedHeaderMap(add_mode);

  EXPECT_EQ(header_map, expected_header_map) << "got header_map:\n"
                                             << header_map << "\nexpected_header_map:\n"
                                             << expected_header_map;
}

TEST(ApplyConfigToResponseHeaders, ReplacesHeadersFromEnvoyApiV3Config) {
  HeaderAddMode add_mode = ReplaceOnDuplicateKey;
  nighthawk::server::ResponseOptions configuration = createTestConfiguration(EnvoyApiV3, add_mode);

  TestResponseHeaderMapImpl header_map;
  applyConfigToResponseHeaders(header_map, configuration);
  TestResponseHeaderMapImpl expected_header_map = createExpectedHeaderMap(add_mode);

  EXPECT_EQ(header_map, expected_header_map) << "got header_map:\n"
                                             << header_map << "\nexpected_header_map:\n"
                                             << expected_header_map;
}

TEST(ApplyConfigToResponseHeaders, AppendsHeadersFromEnvoyApiV3Config) {
  HeaderAddMode add_mode = AppendOnDuplicateKey;
  nighthawk::server::ResponseOptions configuration = createTestConfiguration(EnvoyApiV3, add_mode);

  TestResponseHeaderMapImpl header_map;
  applyConfigToResponseHeaders(header_map, configuration);
  TestResponseHeaderMapImpl expected_header_map = createExpectedHeaderMap(add_mode);

  EXPECT_EQ(header_map, expected_header_map) << "got header_map:\n"
                                             << header_map << "\nexpected_header_map:\n"
                                             << expected_header_map;
}

TEST(ApplyConfigToResponseHeaders, ThrowsOnInvalidConfiguration) {
  nighthawk::server::ResponseOptions configuration;
  configuration.add_response_headers();
  configuration.add_v3_response_headers();

  TestResponseHeaderMapImpl header_map;
  EXPECT_THROW(applyConfigToResponseHeaders(header_map, configuration), Envoy::EnvoyException);
}

TEST(ValidateResponseOptions, DoesNotThrowOnEmptyConfiguration) {
  nighthawk::server::ResponseOptions configuration;
  EXPECT_NO_THROW(validateResponseOptions(configuration));
}

TEST(ValidateResponseOptions, DoesNotThrowWhenOnlyEnvoyApiV2ResponseHeadersAreSet) {
  nighthawk::server::ResponseOptions configuration;
  configuration.add_response_headers();
  EXPECT_NO_THROW(validateResponseOptions(configuration));
}

TEST(ValidateResponseOptions, DoesNotThrowWhenOnlyEnvoyApiV3ResponseHeadersAreSet) {
  nighthawk::server::ResponseOptions configuration;
  configuration.add_v3_response_headers();
  EXPECT_NO_THROW(validateResponseOptions(configuration));
}

TEST(ValidateResponseOptions, ThrowsWhenBothEnvoyApiV2AndV3ResponseHeadersAreSet) {
  nighthawk::server::ResponseOptions configuration;
  configuration.add_response_headers();
  configuration.add_v3_response_headers();
  EXPECT_THROW(validateResponseOptions(configuration), Envoy::EnvoyException);
}

} // namespace
} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
