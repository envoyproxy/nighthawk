#include "envoy/config/core/v3/base.pb.h"
#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/scoring_function_impl.pb.h"
#include "api/client/options.pb.h"

#include "absl/status/status.h"
#include "adaptive_load/plugin_loader.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::envoy::config::core::v3::TypedExtensionConfig;
using ::testing::HasSubstr;

// A special value that causes ValidateConfig to return an error when included in the config
// protos of the fake plugins in this file.
const double kBadConfigThreshold = 98765.0;

/**
 * Returns a validation error if the config proto contains kBadConfigThreshold.
 *
 * @param message An Any proto that must wrap a LinearScoringFunctionConfig.
 *
 * @return Status InvalidArgument if threshold is kBadConfigThreshold, OK otherwise.
 */
absl::Status DoValidateConfig(const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return config.threshold() == kBadConfigThreshold
             ? absl::InvalidArgumentError("input validation failed")
             : absl::OkStatus();
}

/**
 * InputVariableSetter for testing.
 */
class TestInputVariableSetter : public InputVariableSetter {
public:
  // Any plugin in the adaptive load system can freely choose an arbitrary single proto as its
  // config type. We use LinearScoringFunctionConfig for all plugins in this test.
  TestInputVariableSetter(const nighthawk::adaptive_load::LinearScoringFunctionConfig& config)
      : value_from_config_proto_{config.threshold()} {}

  absl::Status SetInputVariable(nighthawk::client::CommandLineOptions& command_line_options,
                                double input_value) override {
    command_line_options.mutable_connections()->set_value(static_cast<unsigned int>(input_value));
    return absl::OkStatus();
  }

  const double value_from_config_proto_;
};

/**
 * A factory that creates a TestInputVariableSetter from a LinearScoringFunctionConfig (see
 * TestInputVariableSetter constructor).
 */
class TestInputVariableSetterConfigFactory : public InputVariableSetterConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-input-variable-setter"; }
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const override {
    return DoValidateConfig(message);
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<nighthawk::adaptive_load::LinearScoringFunctionConfig>();
  }

  InputVariableSetterPtr
  createInputVariableSetter(const Envoy::Protobuf::Message& message) override {
    const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
    nighthawk::adaptive_load::LinearScoringFunctionConfig config;
    Envoy::MessageUtil::unpackTo(any, config);
    return std::make_unique<TestInputVariableSetter>(config);
  }
};

REGISTER_FACTORY(TestInputVariableSetterConfigFactory, InputVariableSetterConfigFactory);

/**
 * ScoringFunction for testing.
 */
class TestScoringFunction : public ScoringFunction {
public:
  // Any plugin in the adaptive load system can freely choose an arbitrary single proto as its
  // config type. We use LinearScoringFunctionConfig for all plugins in this test.
  TestScoringFunction(const nighthawk::adaptive_load::LinearScoringFunctionConfig& config)
      : value_from_config_proto_{config.threshold()} {}

  double EvaluateMetric(double) const override { return 1.0; }

  const double value_from_config_proto_;
};

/**
 * A factory that creates a TestScoringFunction from a LinearScoringFunctionConfig (see
 * TestScoringFunction constructor).
 */
class TestScoringFunctionConfigFactory : public ScoringFunctionConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-scoring-function"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<nighthawk::adaptive_load::LinearScoringFunctionConfig>();
  }
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const override {
    return DoValidateConfig(message);
  }
  ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& message) override {
    const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
    nighthawk::adaptive_load::LinearScoringFunctionConfig config;
    Envoy::MessageUtil::unpackTo(any, config);
    return std::make_unique<TestScoringFunction>(config);
  }
};

REGISTER_FACTORY(TestScoringFunctionConfigFactory, ScoringFunctionConfigFactory);

/**
 * MetricsPlugin for testing.
 */
class TestMetricsPlugin : public MetricsPlugin {
public:
  // Any plugin in the adaptive load system can freely choose an arbitrary single proto as its
  // config type. We use LinearScoringFunctionConfig for all plugins in this test.
  TestMetricsPlugin(const nighthawk::adaptive_load::LinearScoringFunctionConfig& config)
      : value_from_config_proto_{config.threshold()} {}

  absl::StatusOr<double> GetMetricByName(absl::string_view) override { return 5.0; }
  const std::vector<std::string> GetAllSupportedMetricNames() const override { return {}; }

  const double value_from_config_proto_;
};

/**
 * A factory that creates a TestMetricsPlugin from a LinearScoringFunctionConfig (see
 * TestInputVariableSetter constructor).
 */
class TestMetricsPluginConfigFactory : public MetricsPluginConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-metrics-plugin"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<nighthawk::adaptive_load::LinearScoringFunctionConfig>();
  }
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const override {
    return DoValidateConfig(message);
  }
  MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message& message) override {
    const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
    nighthawk::adaptive_load::LinearScoringFunctionConfig config;
    Envoy::MessageUtil::unpackTo(any, config);
    return std::make_unique<TestMetricsPlugin>(config);
  }
};

REGISTER_FACTORY(TestMetricsPluginConfigFactory, MetricsPluginConfigFactory);

/**
 * StepController for testing.
 */
class TestStepController : public StepController {
public:
  // Any plugin in the adaptive load system can freely choose an arbitrary single proto as its
  // config type. We use LinearScoringFunctionConfig for all plugins in this test.
  TestStepController(const nighthawk::adaptive_load::LinearScoringFunctionConfig& config,
                     const nighthawk::client::CommandLineOptions& command_line_options_template)
      : value_from_config_proto_{config.threshold()},
        value_from_command_line_options_template_{
            command_line_options_template.requests_per_second().value()} {}

  bool IsConverged() const override { return false; }
  bool IsDoomed(std::string&) const override { return false; }
  absl::StatusOr<nighthawk::client::CommandLineOptions>
  GetCurrentCommandLineOptions() const override {
    return nighthawk::client::CommandLineOptions();
  }
  void UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult&) override {}

  const double value_from_config_proto_;
  const unsigned int value_from_command_line_options_template_;
};

/**
 * A factory that creates a TestStepController from a LinearScoringFunctionConfig (see
 * TestInputVariableSetter constructor).
 */
class TestStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-step-controller"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<nighthawk::adaptive_load::LinearScoringFunctionConfig>();
  }
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const override {
    return DoValidateConfig(message);
  }
  StepControllerPtr createStepController(
      const Envoy::Protobuf::Message& message,
      const nighthawk::client::CommandLineOptions& command_line_options_template) override {
    const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
    nighthawk::adaptive_load::LinearScoringFunctionConfig config;
    Envoy::MessageUtil::unpackTo(any, config);
    return std::make_unique<TestStepController>(config, command_line_options_template);
  }
};

REGISTER_FACTORY(TestStepControllerConfigFactory, StepControllerConfigFactory);

/**
 * Creates an Any wrapping a TypedExtensionConfig for use in the |typed_config| of all test
 * plugins in this file. The choice of the particular proto LinearScoringFunctionConfig is
 * arbitrary. We don't leave the Any empty because we need to check that the plugin utils can
 * correctly pass the proto through to the plugin.
 */
Envoy::ProtobufWkt::Any CreateTypedConfigAny(const double threshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(threshold);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  return config_any;
}

TEST(PluginUtilTest, CreatesCorrectInputVariableSetterType) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-input-variable-setter");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  InputVariableSetterPtr plugin = LoadInputVariableSetterPlugin(config).value();
  auto* typed_plugin = dynamic_cast<TestInputVariableSetter*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, ReturnsErrorFromInputVariableSetterConfigValidator) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-input-variable-setter");
  *config.mutable_typed_config() = CreateTypedConfigAny(kBadConfigThreshold);
  EXPECT_THAT(LoadInputVariableSetterPlugin(config).status().message(),
              HasSubstr("input validation failed"));
}

TEST(PluginUtilTest, PropagatesConfigProtoToInputVariableSetter) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-input-variable-setter");
  *config.mutable_typed_config() = CreateTypedConfigAny(12.0);
  InputVariableSetterPtr plugin = LoadInputVariableSetterPlugin(config).value();
  auto* typed_plugin = dynamic_cast<TestInputVariableSetter*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->value_from_config_proto_, 12.0);
}

TEST(PluginUtilTest, ReturnsErrorWhenInputVariableSetterPluginNotFound) {
  TypedExtensionConfig config;
  config.set_name("nonexistent-input-variable-setter");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  EXPECT_THAT(LoadInputVariableSetterPlugin(config).status().message(),
              HasSubstr("Didn't find a registered implementation"));
}

TEST(PluginUtilTest, CreatesCorrectScoringFunctionType) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-scoring-function");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  ScoringFunctionPtr plugin = LoadScoringFunctionPlugin(config).value();
  auto* typed_plugin = dynamic_cast<TestScoringFunction*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, ReturnsErrorFromScoringFunctionConfigValidator) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-scoring-function");
  *config.mutable_typed_config() = CreateTypedConfigAny(kBadConfigThreshold);
  EXPECT_THAT(LoadScoringFunctionPlugin(config).status().message(),
              HasSubstr("input validation failed"));
}

TEST(PluginUtilTest, PropagatesConfigProtoToScoringFunction) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-scoring-function");
  *config.mutable_typed_config() = CreateTypedConfigAny(34.0);
  ScoringFunctionPtr plugin = LoadScoringFunctionPlugin(config).value();
  auto* typed_plugin = dynamic_cast<TestScoringFunction*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->value_from_config_proto_, 34.0);
}

TEST(PluginUtilTest, ReturnsErrorWhenScoringFunctionPluginNotFound) {
  TypedExtensionConfig config;
  config.set_name("nonexistent-scoring-function");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  EXPECT_THAT(LoadScoringFunctionPlugin(config).status().message(),
              HasSubstr("Didn't find a registered implementation"));
}

TEST(PluginUtilTest, CreatesCorrectMetricsPluginType) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-metrics-plugin");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  MetricsPluginPtr plugin = LoadMetricsPlugin(config).value();
  auto* typed_plugin = dynamic_cast<TestMetricsPlugin*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, ReturnsErrorFromMetricsPluginConfigValidator) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-metrics-plugin");
  *config.mutable_typed_config() = CreateTypedConfigAny(kBadConfigThreshold);
  EXPECT_THAT(LoadMetricsPlugin(config).status().message(), HasSubstr("input validation failed"));
}

TEST(PluginUtilTest, PropagatesConfigProtoToMetricsPlugin) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-metrics-plugin");
  *config.mutable_typed_config() = CreateTypedConfigAny(56.0);
  MetricsPluginPtr plugin = LoadMetricsPlugin(config).value();
  auto* typed_plugin = dynamic_cast<TestMetricsPlugin*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->value_from_config_proto_, 56.0);
}

TEST(PluginUtilTest, ReturnsErrorWhenMetricsPluginNotFound) {
  TypedExtensionConfig config;
  config.set_name("nonexistent-metrics-plugin");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  EXPECT_THAT(LoadMetricsPlugin(config).status().message(),
              HasSubstr("Didn't find a registered implementation"));
}

TEST(PluginUtilTest, CreatesCorrectStepControllerType) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  nighthawk::client::CommandLineOptions options_template;
  StepControllerPtr plugin = LoadStepControllerPlugin(config, options_template).value();
  auto* typed_plugin = dynamic_cast<TestStepController*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, ReturnsErrorFromStepControllerConfigValidator) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(kBadConfigThreshold);
  nighthawk::client::CommandLineOptions options_template;
  EXPECT_THAT(LoadStepControllerPlugin(config, options_template).status().message(),
              HasSubstr("input validation failed"));
}

TEST(PluginUtilTest, PropagatesConfigProtoToStepController) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(78.0);
  nighthawk::client::CommandLineOptions options_template;
  StepControllerPtr plugin = LoadStepControllerPlugin(config, options_template).value();
  auto* typed_plugin = dynamic_cast<TestStepController*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->value_from_config_proto_, 78.0);
}

TEST(PluginUtilTest, PropagatesCommandLineOptionsTemplateToStepController) {
  TypedExtensionConfig config;
  config.set_name("nighthawk.test-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  nighthawk::client::CommandLineOptions options_template;
  options_template.mutable_requests_per_second()->set_value(9);
  StepControllerPtr plugin = LoadStepControllerPlugin(config, options_template).value();
  auto* typed_plugin = dynamic_cast<TestStepController*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->value_from_command_line_options_template_, 9);
}

TEST(PluginUtilTest, ReturnsErrorWhenStepControllerPluginNotFound) {
  TypedExtensionConfig config;
  config.set_name("nonexistent-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  nighthawk::client::CommandLineOptions options_template;
  EXPECT_THAT(LoadStepControllerPlugin(config, options_template).status().message(),
              HasSubstr("Didn't find a registered implementation"));
}

} // namespace

} // namespace Nighthawk
