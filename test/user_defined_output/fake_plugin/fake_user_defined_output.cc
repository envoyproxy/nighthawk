#include "test/user_defined_output/fake_plugin/fake_user_defined_output.h"

#include "test/user_defined_output/fake_plugin/fake_user_defined_output.pb.h"

namespace Nighthawk {

using ::nighthawk::FakeUserDefinedOutput;
using ::nighthawk::FakeUserDefinedOutputConfig;

FakeUserDefinedOutputPlugin::FakeUserDefinedOutputPlugin(
    FakeUserDefinedOutputConfig config,
    WorkerMetadata worker_metadata)
    : config_(std::move(config)), worker_metadata_(std::move(worker_metadata)) {}

absl::Status FakeUserDefinedOutputPlugin::handleResponseHeaders(
  const Envoy::Http::ResponseHeaderMapPtr&&) {
  headers_called_++;

  return absl::OkStatus();
}

absl::Status FakeUserDefinedOutputPlugin::handleResponseData(const Envoy::Buffer::Instance&) {
  data_called_++;

  return absl::OkStatus();
}

absl::StatusOr<google::protobuf::Any> FakeUserDefinedOutputPlugin::getPerWorkerOutput() {
  FakeUserDefinedOutput output;
  output.set_data_called(data_called_);
  output.set_headers_called(headers_called_);
  output.set_worker_name(worker_metadata_.name);

  google::protobuf::Any output_any;
  output_any.PackFrom(output);
  return output_any;
}

std::string FakeUserDefinedOutputPluginFactory::name() const {
  return "nighthawk.fake_user_defined_output";
}
Envoy::ProtobufTypes::MessagePtr FakeUserDefinedOutputPluginFactory::createEmptyConfigProto() {
  return std::make_unique<FakeUserDefinedOutputConfig>();
}

UserDefinedOutputPluginPtr FakeUserDefinedOutputPluginFactory::createUserDefinedOutputPlugin(
    const Envoy::Protobuf::Message& message,
    const WorkerMetadata& worker_metadata) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  FakeUserDefinedOutputConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FakeUserDefinedOutputPlugin>(config, worker_metadata);
}

absl::StatusOr<google::protobuf::Any> FakeUserDefinedOutputPluginFactory::AggregateGlobalOutput(
  absl::Span<const google::protobuf::Any> per_worker_outputs) {
  FakeUserDefinedOutput global_output;
  global_output.set_worker_name("global");
  int data_called = 0;
  int headers_called = 0;
  for (const google::protobuf::Any& any : per_worker_outputs) {
    FakeUserDefinedOutput output;
    if (any.UnpackTo(&output)) {
      data_called += output.data_called();
      headers_called += output.headers_called();
    }
    else {
      return absl::InternalError(absl::StrCat(
        "Unable to unpack Any into a FakeUserDefinedOutput: ",
        any.ShortDebugString()
      ));
    }
  }

  global_output.set_data_called(data_called);
  global_output.set_headers_called(headers_called);

  google::protobuf::Any global_any;
  global_any.PackFrom(global_output);

  return global_any;
}

REGISTER_FACTORY(FakeUserDefinedOutputPluginFactory, UserDefinedOutputPluginFactory);

} // namespace Nighthawk
