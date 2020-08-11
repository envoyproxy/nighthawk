#include "envoy/common/exception.h"

#include "external/envoy/source/common/config/utility.h"

#include "adaptive_load/scoring_function_impl.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

TEST(BinaryScoringFunctionConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary-scoring");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::adaptive_load::BinaryScoringFunctionConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}

TEST(LinearScoringFunctionConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear-scoring");
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
          "nighthawk.binary-scoring");
  EXPECT_EQ(config_factory.name(), "nighthawk.binary-scoring");
}

TEST(BinaryScoringFunctionConfigFactory, CreateScoringFunctionCreatesCorrectPluginType) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary-scoring");
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
          "nighthawk.binary-scoring");
  EXPECT_THROW(config_factory.createScoringFunction(config_any), Envoy::EnvoyException);
}

TEST(LinearScoringFunctionConfigFactory, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear-scoring");
  EXPECT_EQ(config_factory.name(), "nighthawk.linear-scoring");
}

TEST(LinearScoringFunctionConfigFactory, CreateScoringFunctionCreatesCorrectPluginType) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear-scoring");
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
          "nighthawk.linear-scoring");
  EXPECT_THROW(config_factory.createScoringFunction(config_any), Envoy::EnvoyException);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsOneForValueWithinUpperThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_upper_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(4.0), 1.0);
}

TEST(BinaryScoringFunction, EvaluateMetriceturnsOneForValueEqualToUpperThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_upper_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(5.0), 1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsNegativeOneForValueOutsideUpperThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_upper_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(6.0), -1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsOneForValueWithinLowerThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(6.0), 1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsOneForValueEqualToLowerThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(5.0), 1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsNegativeOneForValueOutsideLowerThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(4.0), -1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsOneForValueWithinThresholdRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(6.0), 1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsOneForValueEqualToLowerThresholdOfRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(5.0), 1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsOneForValueEqualToUpperThresholdOfRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(7.0), 1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsNegativeOneForValueBelowThresholdRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(4.0), -1.0);
}

TEST(BinaryScoringFunction, EvaluateMetricReturnsNegativeOneForValueAboveThresholdRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(8.0), -1.0);
}

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
