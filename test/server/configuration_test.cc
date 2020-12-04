#include "envoy/api/v2/core/base.pb.h"
#include "envoy/config/core/v3/base.pb.h"

#include "server/configuration.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace Server {
namespace Configuration {
namespace {

using ::testing::HasSubstr;

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

} // namespace
} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
