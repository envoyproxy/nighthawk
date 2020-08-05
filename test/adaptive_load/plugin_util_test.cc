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

#include "adaptive_load/plugin_util.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

/**
 * InputVariableSetter for testing.
 */
class TestInputVariableSetter : public InputVariableSetter {
public:
  // Any plugin in the adaptive load system can freely choose an arbitrary single proto as its
  // config type. We use LinearScoringFunctionConfig for all plugins in this test.
  TestInputVariableSetter(const nighthawk::adaptive_load::LinearScoringFunctionConfig& config)
      : config_{config} {}
  void SetInputVariable(nighthawk::client::CommandLineOptions& command_line_options,
                        double input_value) override {
    command_line_options.mutable_connections()->set_value(static_cast<unsigned int>(input_value));
  }
  const nighthawk::adaptive_load::LinearScoringFunctionConfig config_;
};

/**
 * A factory that creates a TestInputVariableSetter from a LinearScoringFunctionConfig (see
 * TestInputVariableSetter constructor).
 */
class TestInputVariableSetterConfigFactory : public InputVariableSetterConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-input-variable-setter"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  InputVariableSetterPtr
  createInputVariableSetter(const Envoy::Protobuf::Message& message) override {
    const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
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
      : config_{config} {}
  double EvaluateMetric(double) const override { return 1.0; }
  const nighthawk::adaptive_load::LinearScoringFunctionConfig config_;
};

/**
 * A factory that creates a TestScoringFunction from a LinearScoringFunctionConfig (see
 * TestScoringFunction constructor).
 */
class TestScoringFunctionConfigFactory : public ScoringFunctionConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-scoring-function"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& message) override {
    const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
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
      : config_{config} {}
  Envoy::StatusOr<double> GetMetricByName(absl::string_view) override { return 5.0; }
  const std::vector<std::string> GetAllSupportedMetricNames() const override { return {}; }
  const nighthawk::adaptive_load::LinearScoringFunctionConfig config_;
};

/**
 * A factory that creates a TestMetricsPlugin from a LinearScoringFunctionConfig (sic).
 */
class TestMetricsPluginConfigFactory : public MetricsPluginConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-metrics-plugin"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message& message) override {
    const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
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
      : config_{config}, command_line_options_template_{command_line_options_template} {}
  bool IsConverged() const override { return false; }
  bool IsDoomed(std::string&) const override { return false; }
  nighthawk::client::CommandLineOptions GetCurrentCommandLineOptions() const override {
    return nighthawk::client::CommandLineOptions();
  }
  void UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult&) override {}
  const nighthawk::adaptive_load::LinearScoringFunctionConfig config_;
  const nighthawk::client::CommandLineOptions command_line_options_template_;
};

/**
 * A factory that creates a TestStepController from a LinearScoringFunctionConfig (sic).
 */
class TestStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override { return "nighthawk.test-step-controller"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  StepControllerPtr createStepController(
      const Envoy::Protobuf::Message& message,
      const nighthawk::client::CommandLineOptions& command_line_options_template) override {
    const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
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
Envoy::ProtobufWkt::Any CreateTypedConfigAny(double threshold) {
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  config.set_threshold(threshold);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  return config_any;
}

TEST(PluginUtilTest, CreatesCorrectInputVariableSetterType) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-input-variable-setter");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  InputVariableSetterPtr plugin = LoadInputVariableSetterPlugin(config);
  TestInputVariableSetter* typed_plugin = dynamic_cast<TestInputVariableSetter*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, PropagatesConfigProtoToInputVariableSetter) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-input-variable-setter");
  *config.mutable_typed_config() = CreateTypedConfigAny(12.0);
  InputVariableSetterPtr plugin = LoadInputVariableSetterPlugin(config);
  TestInputVariableSetter* typed_plugin = dynamic_cast<TestInputVariableSetter*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->config_.threshold(), 12.0);
}

TEST(PluginUtilTest, ThrowsExceptionWhenInputVariableSetterPluginNotFound) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nonexistent-input-variable-setter");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  EXPECT_THROW(LoadInputVariableSetterPlugin(config), Envoy::EnvoyException);
}

TEST(PluginUtilTest, CreatesCorrectScoringFunctionType) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-scoring-function");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  ScoringFunctionPtr plugin = LoadScoringFunctionPlugin(config);
  TestScoringFunction* typed_plugin = dynamic_cast<TestScoringFunction*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, PropagatesConfigProtoToScoringFunction) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-scoring-function");
  *config.mutable_typed_config() = CreateTypedConfigAny(34.0);
  ScoringFunctionPtr plugin = LoadScoringFunctionPlugin(config);
  TestScoringFunction* typed_plugin = dynamic_cast<TestScoringFunction*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->config_.threshold(), 34.0);
}

TEST(PluginUtilTest, ThrowsExceptionWhenScoringFunctionPluginNotFound) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nonexistent-scoring-function");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  EXPECT_THROW(LoadScoringFunctionPlugin(config), Envoy::EnvoyException);
}

TEST(PluginUtilTest, CreatesCorrectMetricsPluginType) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-metrics-plugin");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  MetricsPluginPtr plugin = LoadMetricsPlugin(config);
  TestMetricsPlugin* typed_plugin = dynamic_cast<TestMetricsPlugin*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, PropagatesConfigProtoToMetricsPlugin) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-metrics-plugin");
  *config.mutable_typed_config() = CreateTypedConfigAny(56.0);
  MetricsPluginPtr plugin = LoadMetricsPlugin(config);
  TestMetricsPlugin* typed_plugin = dynamic_cast<TestMetricsPlugin*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->config_.threshold(), 56.0);
}

TEST(PluginUtilTest, ThrowsExceptionWhenMetricsPluginNotFound) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nonexistent-metrics-plugin");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  EXPECT_THROW(LoadMetricsPlugin(config), Envoy::EnvoyException);
}

TEST(PluginUtilTest, CreatesCorrectStepControllerType) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  nighthawk::client::CommandLineOptions options_template;
  StepControllerPtr plugin = LoadStepControllerPlugin(config, options_template);
  TestStepController* typed_plugin = dynamic_cast<TestStepController*>(plugin.get());
  EXPECT_NE(typed_plugin, nullptr);
}

TEST(PluginUtilTest, PropagatesConfigProtoToStepController) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(78.0);
  nighthawk::client::CommandLineOptions options_template;
  StepControllerPtr plugin = LoadStepControllerPlugin(config, options_template);
  TestStepController* typed_plugin = dynamic_cast<TestStepController*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->config_.threshold(), 78.0);
}

TEST(PluginUtilTest, PropagatesCommandLineOptionsTemplateToStepController) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.test-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  nighthawk::client::CommandLineOptions options_template;
  options_template.mutable_requests_per_second()->set_value(9);
  StepControllerPtr plugin = LoadStepControllerPlugin(config, options_template);
  TestStepController* typed_plugin = dynamic_cast<TestStepController*>(plugin.get());
  ASSERT_NE(typed_plugin, nullptr);
  EXPECT_EQ(typed_plugin->command_line_options_template_.requests_per_second().value(), 9);
}

TEST(PluginUtilTest, ThrowsExceptionWhenStepControllerPluginNotFound) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nonexistent-step-controller");
  *config.mutable_typed_config() = CreateTypedConfigAny(0.0);
  nighthawk::client::CommandLineOptions options_template;
  EXPECT_THROW(LoadStepControllerPlugin(config, options_template), Envoy::EnvoyException);
}

} // namespace

} // namespace Nighthawk