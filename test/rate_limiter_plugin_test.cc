#include <chrono>
#include <string>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nighthawk/common/exception.h"
#include "external/envoy/source/common/config/utility.h"

#include "envoy/api/api.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/test_common/simulated_time_system.h"
#include "test/mocks/client/mock_options.h"
#include "external/envoy/test/test_common/utility.h"

#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/rate_limiter_plugin_config_factory.h"
#include "api/rate_limiter/linear_ramping_rate_limiter.pb.h"
#include "source/common/rate_limiter_impl.h"

#include "test/test_common/proto_matchers.h"

namespace Nighthawk {
namespace Client {

class LinearRampingRateLimiterPluginTest : public testing::Test {
public:
  LinearRampingRateLimiterPluginTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  testing::NiceMock<MockOptions> options_;
  const std::string plugin_name_ = "nighthawk.linear-ramping-rate-limiter-plugin";
};

TEST_F(LinearRampingRateLimiterPluginTest, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RateLimiterPluginConfigFactory>(
          plugin_name_);
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::rate_limiter::LinearRampingRateLimiterConfig expected_config;
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));
}

TEST_F(LinearRampingRateLimiterPluginTest, FactoryRegistrationUsesCorrectPluginName) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RateLimiterPluginConfigFactory>(
          plugin_name_);
  EXPECT_EQ(config_factory.name(), plugin_name_);
}

TEST_F(LinearRampingRateLimiterPluginTest, ValidConfigInitializesWorkingRateLimiter) {
  const unsigned int requests_per_second = 100;
  const unsigned int ramp_time = 5;
  const unsigned int duration = 5;

  // Verify factory is initialized correctly from plugin name.
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RateLimiterPluginConfigFactory>(
          plugin_name_);

  EXPECT_EQ(config_factory.name(), plugin_name_);

  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::rate_limiter::LinearRampingRateLimiterConfig expected_config;
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));

  // Verify rate limiter is initialized correctly from config proto.
  nighthawk::rate_limiter::LinearRampingRateLimiterConfig config;
  config.mutable_ramp_time()->set_seconds(ramp_time);
  Envoy::Protobuf::Any config_any;
  std::ignore = config_any.PackFrom(config);

  EXPECT_CALL(options_, requestsPerSecond()).WillOnce(testing::Return(requests_per_second));
  EXPECT_CALL(options_, noDuration()).WillOnce(testing::Return(false));
  EXPECT_CALL(options_, duration()).WillOnce(testing::Return(std::chrono::seconds(duration)));

  RateLimiterPtr plugin =
      config_factory.createRateLimiterPlugin(config_any, *api_, time_system_, options_);

  ASSERT_NE(plugin, nullptr);
  EXPECT_NE(dynamic_cast<LinearRampingRateLimiterImpl*>(plugin.get()), nullptr);
}

TEST_F(LinearRampingRateLimiterPluginTest, RampTimeExceedingDurationThrowsException) {
  LinearRampingRateLimiterImplFactory config_factory;

  nighthawk::rate_limiter::LinearRampingRateLimiterConfig config;
  config.mutable_ramp_time()->set_seconds(15);
  Envoy::Protobuf::Any config_any;
  std::ignore = config_any.PackFrom(config);

  EXPECT_CALL(options_, requestsPerSecond()).WillOnce(testing::Return(100));
  EXPECT_CALL(options_, noDuration()).WillOnce(testing::Return(false));
  EXPECT_CALL(options_, duration()).WillOnce(testing::Return(std::chrono::seconds(10)));

  EXPECT_THROW_WITH_REGEX(
      config_factory.createRateLimiterPlugin(config_any, *api_, time_system_, options_),
      NighthawkException, "ramp_time must be less than or equal to time specified by --duration");
}

TEST_F(LinearRampingRateLimiterPluginTest, NoDurationInitializesCorrectly) {
  LinearRampingRateLimiterImplFactory config_factory;

  nighthawk::rate_limiter::LinearRampingRateLimiterConfig config;
  config.mutable_ramp_time()->set_seconds(100);
  Envoy::Protobuf::Any config_any;
  std::ignore = config_any.PackFrom(config);

  EXPECT_CALL(options_, requestsPerSecond()).WillOnce(testing::Return(100));
  EXPECT_CALL(options_, noDuration()).WillOnce(testing::Return(true));

  RateLimiterPtr plugin =
      config_factory.createRateLimiterPlugin(config_any, *api_, time_system_, options_);
  ASSERT_NE(plugin, nullptr);
}

TEST_F(LinearRampingRateLimiterPluginTest, ZeroRampTimeThrowsException) {
  LinearRampingRateLimiterImplFactory config_factory;

  nighthawk::rate_limiter::LinearRampingRateLimiterConfig config;
  config.mutable_ramp_time()->set_seconds(0);
  config.mutable_ramp_time()->set_nanos(0);
  Envoy::Protobuf::Any config_any;
  std::ignore = config_any.PackFrom(config);

  EXPECT_CALL(options_, requestsPerSecond()).WillOnce(testing::Return(100));

  EXPECT_THROW_WITH_REGEX(
      config_factory.createRateLimiterPlugin(config_any, *api_, time_system_, options_),
      NighthawkException, "ramp_time must be positive");
}

} // namespace Client
} // namespace Nighthawk