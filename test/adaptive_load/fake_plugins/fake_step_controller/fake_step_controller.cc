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
    const nighthawk::adaptive_load::FakeStepControllerConfig& config,
    nighthawk::client::CommandLineOptions command_line_options_template)
    : is_converged_{false}, is_doomed_{false}, fixed_rps_value_{config.fixed_rps_value()},
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
  nighthawk::client::CommandLineOptions options = command_line_options_template_;
  options.mutable_requests_per_second()->set_value(fixed_rps_value_);
  return options;
}

void FakeStepController::UpdateAndRecompute(
    const nighthawk::adaptive_load::BenchmarkResult& benchmark_result) {
  if (benchmark_result.status().code() == ::grpc::OK) {
    is_doomed_ = false;
    doomed_reason_ = "";
  } else {
    is_doomed_ = true;
    doomed_reason_ = benchmark_result.status().message();
  }
  // "Convergence" is defined as the latest benchmark reporting any score > 0.0.
  is_converged_ = false;
  for (const nighthawk::adaptive_load::MetricEvaluation& metric_evaluation :
       benchmark_result.metric_evaluations()) {
    if (metric_evaluation.threshold_score() > 0.0) {
      is_converged_ = true;
      break;
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

} // namespace Nighthawk
