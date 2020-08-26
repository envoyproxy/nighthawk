#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.h"

#include "api/adaptive_load/benchmark_result.pb.h"

#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.pb.h"

namespace Nighthawk {

namespace {

absl::Status StatusFromProtoRpcStatus(const google::rpc::Status& status_proto) {
  return absl::Status(static_cast<absl::StatusCode>(status_proto.code()), status_proto.message());
}

} // namespace

FakeStepController::FakeStepController(
    nighthawk::adaptive_load::FakeStepControllerConfig config,
    nighthawk::client::CommandLineOptions command_line_options_template)
    : input_setting_failure_countdown_{config.artificial_input_setting_failure_countdown()},
      config_{std::move(config)}, is_converged_{false}, is_doomed_{false},
      command_line_options_template_{std::move(command_line_options_template)} {}

bool FakeStepController::IsConverged() const { return is_converged_; }

bool FakeStepController::IsDoomed(std::string& doomed_reason) const {
  if (is_doomed_) {
    doomed_reason = doomed_reason_;
    return true;
  } else {
    return false;
  }
}

absl::StatusOr<nighthawk::client::CommandLineOptions>
FakeStepController::GetCurrentCommandLineOptions() const {
  if (config_.has_artificial_input_setting_failure() && input_setting_failure_countdown_ <= 0) {
    return StatusFromProtoRpcStatus(config_.artificial_input_setting_failure());
  }
  nighthawk::client::CommandLineOptions options = command_line_options_template_;
  options.mutable_requests_per_second()->set_value(config_.fixed_rps_value());
  return options;
}

void FakeStepController::UpdateAndRecompute(
    const nighthawk::adaptive_load::BenchmarkResult& benchmark_result) {
  if (input_setting_failure_countdown_ > 0) {
    --input_setting_failure_countdown_;
  }
  // "Convergence" is defined as the latest benchmark reporting any score > 0.0.
  // "Doom" is defined as any score < 0.0. Neutral is all scores equal to 0.0.
  is_converged_ = false;
  is_doomed_ = false;
  doomed_reason_ = "";
  for (const nighthawk::adaptive_load::MetricEvaluation& metric_evaluation :
       benchmark_result.metric_evaluations()) {
    if (metric_evaluation.threshold_score() < 0.0) {
      is_doomed_ = true;
      doomed_reason_ = "artificial doom triggered by negative score";
    } else if (metric_evaluation.threshold_score() > 0.0) {
      is_converged_ = true;
    }
  }
}

std::string FakeStepControllerConfigFactory::name() const {
  return "nighthawk.fake_step_controller";
}
Envoy::ProtobufTypes::MessagePtr FakeStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::FakeStepControllerConfig>();
}

StepControllerPtr FakeStepControllerConfigFactory::createStepController(
    const Envoy::Protobuf::Message& message,
    const nighthawk::client::CommandLineOptions& command_line_options_template) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::FakeStepControllerConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FakeStepController>(config, command_line_options_template);
}

absl::Status
FakeStepControllerConfigFactory::ValidateConfig(const Envoy::Protobuf::Message& message) const {
  try {
    const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
    nighthawk::adaptive_load::FakeStepControllerConfig config;
    Envoy::MessageUtil::unpackTo(any, config);
    if (config.has_artificial_validation_failure()) {
      return StatusFromProtoRpcStatus(config.artificial_validation_failure());
    }
    return absl::OkStatus();
  } catch (const Envoy::EnvoyException& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse FakeStepControllerConfig proto: ", e.what()));
  }
}

REGISTER_FACTORY(FakeStepControllerConfigFactory, StepControllerConfigFactory);

envoy::config::core::v3::TypedExtensionConfig
MakeFakeStepControllerPluginConfig(int fixed_rps_value) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake_step_controller");
  nighthawk::adaptive_load::FakeStepControllerConfig config;
  config.set_fixed_rps_value(fixed_rps_value);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  *outer_config.mutable_typed_config() = config_any;
  return outer_config;
}

envoy::config::core::v3::TypedExtensionConfig MakeFakeStepControllerPluginConfigWithValidationError(
    const absl::Status& artificial_validation_error) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake_step_controller");
  nighthawk::adaptive_load::FakeStepControllerConfig config;
  config.mutable_artificial_validation_failure()->set_code(
      static_cast<int>(artificial_validation_error.code()));
  config.mutable_artificial_validation_failure()->set_message(
      std::string(artificial_validation_error.message()));
  outer_config.mutable_typed_config()->PackFrom(config);
  return outer_config;
}

envoy::config::core::v3::TypedExtensionConfig
MakeFakeStepControllerPluginConfigWithInputSettingError(
    const absl::Status& artificial_input_setting_failure, int countdown) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake_step_controller");
  nighthawk::adaptive_load::FakeStepControllerConfig config;
  config.mutable_artificial_input_setting_failure()->set_code(
      static_cast<int>(artificial_input_setting_failure.code()));
  config.mutable_artificial_input_setting_failure()->set_message(
      std::string(artificial_input_setting_failure.message()));
  config.set_artificial_input_setting_failure_countdown(countdown);
  outer_config.mutable_typed_config()->PackFrom(config);
  return outer_config;
}

} // namespace Nighthawk
