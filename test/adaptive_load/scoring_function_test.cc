#include "external/envoy/source/common/config/utility.h"

#include "adaptive_load/plugin_util.h"
#include "adaptive_load/scoring_function_impl.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace AdaptiveLoad {
namespace {

TEST(BinaryScoringFunctionConfigFactoryTest, GeneratesEmptyConfigProto) {
  ScoringFunctionConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();

  nighthawk::adaptive_load::BinaryScoringFunctionConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(LinearScoringFunctionConfigFactoryTest, GeneratesEmptyConfigProto) {
  ScoringFunctionConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();

  nighthawk::adaptive_load::LinearScoringFunctionConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(SigmoidScoringFunctionConfigFactoryTest, GeneratesEmptyConfigProto) {
  ScoringFunctionConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.sigmoid");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();

  nighthawk::adaptive_load::SigmoidScoringFunctionConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(BinaryScoringFunctionConfigFactoryTest, CreatesBinaryScoringFunctionPlugin) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);

  ScoringFunctionConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.binary");
  ScoringFunctionPtr plugin = config_factory.createScoringFunction(config_any);

  EXPECT_NE(dynamic_cast<BinaryScoringFunction*>(plugin.get()), nullptr);
}

TEST(LinearScoringFunctionConfigFactoryTest, CreatesLinearScoringFunctionPlugin) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);

  ScoringFunctionConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.linear");
  ScoringFunctionPtr plugin = config_factory.createScoringFunction(config_any);

  EXPECT_NE(dynamic_cast<LinearScoringFunction*>(plugin.get()), nullptr);
}

TEST(SigmoidScoringFunctionConfigFactoryTest, CreatesSigmoidScoringFunctionPlugin) {
  nighthawk::adaptive_load::SigmoidScoringFunctionConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);

  ScoringFunctionConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
          "nighthawk.sigmoid");
  ScoringFunctionPtr plugin = config_factory.createScoringFunction(config_any);

  EXPECT_NE(dynamic_cast<SigmoidScoringFunction*>(plugin.get()), nullptr);
}

TEST(BinaryScoringFunctionTest, ReturnsOneForValueWithinUpperThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_upper_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(4.0), 1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsOneForValueEqualToUpperThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_upper_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(5.0), 1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsNegativeOneForValueOutsideUpperThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_upper_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(6.0), -1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsOneForValueWithinLowerThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(6.0), 1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsOneForValueEqualToLowerThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(5.0), 1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsNegativeOneForValueOutsideLowerThreshold) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(4.0), -1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsOneForValueWithinThresholdRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(6.0), 1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsOneForValueEqualToLowerThresholdOfRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(5.0), 1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsOneForValueEqualToUpperThresholdOfRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(7.0), 1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsNegativeOneForValueBelowThresholdRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(4.0), -1.0);
}

TEST(BinaryScoringFunctionTest, ReturnsNegativeOneForValueAboveThresholdRange) {
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  config.mutable_lower_threshold()->set_value(5.0);
  config.mutable_upper_threshold()->set_value(7.0);
  BinaryScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(8.0), -1.0);
}

TEST(LinearScoringFunctionTest, ReturnsZeroForValueEqualToThreshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(10.0);
  config.set_k(1.0);
  LinearScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(10.0), 0.0);
}

TEST(LinearScoringFunctionTest, ReturnsPositiveValueForValueBelowThreshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(10.0);
  config.set_k(1.0);
  LinearScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(8.0), 2.0);
}

TEST(LinearScoringFunctionTest, ReturnsNegativeValueForValueAboveThreshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(10.0);
  config.set_k(1.0);
  LinearScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(12.0), -2.0);
}

TEST(SigmoidScoringFunctionTest, ReturnsZeroForValueEqualToThreshold) {
  nighthawk::adaptive_load::SigmoidScoringFunctionConfig config;
  config.set_threshold(10.0);
  config.set_k(1.0);
  SigmoidScoringFunction scoring_function(config);
  EXPECT_EQ(scoring_function.EvaluateMetric(10.0), 0.0);
}

TEST(SigmoidScoringFunctionTest, ReturnsPositiveValueForValueBelowThreshold) {
  nighthawk::adaptive_load::SigmoidScoringFunctionConfig config;
  config.set_threshold(1000.0);
  config.set_k(0.001);
  SigmoidScoringFunction scoring_function(config);
  EXPECT_GT(scoring_function.EvaluateMetric(999.0), 0.0);
}

TEST(SigmoidScoringFunctionTest, ReturnsNegativeValueForValueAboveThreshold) {
  nighthawk::adaptive_load::SigmoidScoringFunctionConfig config;
  config.set_threshold(1000.0);
  config.set_k(0.001);
  SigmoidScoringFunction scoring_function(config);
  EXPECT_LT(scoring_function.EvaluateMetric(1001.0), 0.0);
}

TEST(SigmoidScoringFunctionTest, ReturnsValueCloseToOneForValueFarBelowThreshold) {
  nighthawk::adaptive_load::SigmoidScoringFunctionConfig config;
  config.set_threshold(1000.0);
  config.set_k(0.001);
  SigmoidScoringFunction scoring_function(config);
  EXPECT_LT(1.0 - scoring_function.EvaluateMetric(-1000000.0), 0.01);
}

TEST(SigmoidScoringFunctionTest, ReturnsValueCloseToNegativeOneForValueFarAboveThreshold) {
  nighthawk::adaptive_load::SigmoidScoringFunctionConfig config;
  config.set_threshold(1000.0);
  config.set_k(0.001);
  SigmoidScoringFunction scoring_function(config);
  EXPECT_LT(1.0 + scoring_function.EvaluateMetric(1000000.0), 0.01);
}

} // namespace
} // namespace AdaptiveLoad
} // namespace Nighthawk