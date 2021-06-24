#include <cmath>

#include "envoy/common/exception.h"

#include "external/envoy/source/common/config/utility.h"

#include "source/adaptive_load/scoring_function_impl.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

TEST(BinaryScoringFunctionConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary_scoring");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::adaptive_load::BinaryScoringFunctionConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}

TEST(LinearScoringFunctionConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear_scoring");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::adaptive_load::LinearScoringFunctionConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}

TEST(BinaryScoringFunctionConfigFactory, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary_scoring");
  EXPECT_EQ(config_factory.name(), "nighthawk.binary_scoring");
}

TEST(BinaryScoringFunctionConfigFactory, CreateScoringFunctionCreatesCorrectPluginType) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary_scoring");
  ScoringFunctionPtr plugin = config_factory.createScoringFunction(config_any);
  EXPECT_NE(dynamic_cast<BinaryScoringFunction*>(plugin.get()), nullptr);
}

TEST(BinaryScoringFunctionConfigFactory, CreateScoringFunctionThrowsExceptionWithWrongConfigProto) {
  // LinearScoringFunctionConfig is the wrong type for BinaryScoringFunction, so it will not unpack.
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary_scoring");
  EXPECT_THROW(config_factory.createScoringFunction(config_any), Envoy::EnvoyException);
}

TEST(LinearScoringFunctionConfigFactory, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear_scoring");
  EXPECT_EQ(config_factory.name(), "nighthawk.linear_scoring");
}

TEST(LinearScoringFunctionConfigFactory, CreateScoringFunctionCreatesCorrectPluginType) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear_scoring");
  ScoringFunctionPtr plugin = config_factory.createScoringFunction(config_any);
  EXPECT_NE(dynamic_cast<LinearScoringFunction*>(plugin.get()), nullptr);
}

TEST(LinearScoringFunctionConfigFactory, CreateScoringFunctionThrowsExceptionWithWrongConfigProto) {
  // BinaryScoringFunctionConfig is the wrong type for LinearScoringFunction, so it will not unpack.
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear_scoring");
  EXPECT_THROW(config_factory.createScoringFunction(config_any), Envoy::EnvoyException);
}

nighthawk::adaptive_load::BinaryScoringFunctionConfig
MakeBinaryConfigWithUpperThreshold(double upper_threshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_upper_threshold()->set_value(upper_threshold);
  return config;
}

nighthawk::adaptive_load::BinaryScoringFunctionConfig
MakeBinaryConfigWithLowerThreshold(double lower_threshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(lower_threshold);
  return config;
}

nighthawk::adaptive_load::BinaryScoringFunctionConfig
MakeBinaryConfigWithBothThresholds(double lower_threshold, double upper_threshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(lower_threshold);
  config.mutable_upper_threshold()->set_value(upper_threshold);
  return config;
}

class BinaryScoringFunctionFixture
    : public testing::TestWithParam<
          std::tuple<nighthawk::adaptive_load::BinaryScoringFunctionConfig, /*metric value*/ double,
                     /*expected score*/ double>> {};

TEST_P(BinaryScoringFunctionFixture, ComputesCorrectScore) {
  const nighthawk::adaptive_load::BinaryScoringFunctionConfig& config = std::get<0>(GetParam());
  const double metric_value = std::get<1>(GetParam());
  const double expected_score = std::get<2>(GetParam());
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(metric_value), expected_score);
}

INSTANTIATE_TEST_SUITE_P(
    BinaryScoringFunctionTest, BinaryScoringFunctionFixture,
    testing::ValuesIn(std::vector<std::tuple<nighthawk::adaptive_load::BinaryScoringFunctionConfig,
                                             /*metric value*/ double,
                                             /*expected score*/ double>>{
        {MakeBinaryConfigWithUpperThreshold(5.0), 4.0, 1.0},
        {MakeBinaryConfigWithUpperThreshold(5.0), 5.0, 1.0},
        {MakeBinaryConfigWithUpperThreshold(5.0), 6.0, -1.0},

        {MakeBinaryConfigWithLowerThreshold(5.0), 4.0, -1.0},
        {MakeBinaryConfigWithLowerThreshold(5.0), 5.0, 1.0},
        {MakeBinaryConfigWithLowerThreshold(5.0), 6.0, 1.0},

        {MakeBinaryConfigWithBothThresholds(5.0, 7.0), 6.0, 1.0},
        {MakeBinaryConfigWithBothThresholds(5.0, 7.0), 5.0, 1.0},
        {MakeBinaryConfigWithBothThresholds(5.0, 7.0), 7.0, 1.0},
        {MakeBinaryConfigWithBothThresholds(5.0, 7.0), 4.0, -1.0},
        {MakeBinaryConfigWithBothThresholds(5.0, 7.0), 8.0, -1.0}}));

TEST(LinearScoringFunction, EvaluateMetricReturnsZeroForValueEqualToThreshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(10.0);
  config.set_scaling_constant(1.0);
  LinearScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(10.0), 0.0);
}

TEST(LinearScoringFunction, EvaluateMetricReturnsPositiveValueForValueBelowThreshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(10.0);
  config.set_scaling_constant(1.0);
  LinearScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(8.0), 2.0);
}

TEST(LinearScoringFunction, EvaluateMetricReturnsNegativeValueForValueAboveThreshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(10.0);
  config.set_scaling_constant(1.0);
  LinearScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(12.0), -2.0);
}

} // namespace

} // namespace Nighthawk
