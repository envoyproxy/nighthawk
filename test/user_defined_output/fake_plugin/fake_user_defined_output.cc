#include "test/user_defined_output/fake_plugin/fake_user_defined_output.h"

#include "test/user_defined_output/fake_plugin/fake_user_defined_output.pb.h"

namespace Nighthawk {

using ::nighthawk::FakeUserDefinedOutput;
using ::nighthawk::FakeUserDefinedOutputConfig;

FakeUserDefinedOutputPlugin::FakeUserDefinedOutputPlugin(FakeUserDefinedOutputConfig config,
                                                         WorkerMetadata worker_metadata)
    : config_(std::move(config)), worker_metadata_(worker_metadata) {}

absl::Status
FakeUserDefinedOutputPlugin::handleResponseHeaders(const Envoy::Http::ResponseHeaderMap&) {
  Envoy::Thread::LockGuard guard(headers_lock_);
  headers_called_++;
  if (config_.fail_headers()) {
    if (headers_called_ > config_.header_failure_countdown()) {
      return absl::InternalError("Intentional FakeUserDefinedOutputPlugin failure on headers");
    }
  }

  return absl::OkStatus();
}

absl::Status FakeUserDefinedOutputPlugin::handleResponseData(const Envoy::Buffer::Instance& data) {
  Envoy::Thread::LockGuard guard(data_lock_);
  if (data.toString().empty()) {
    // handleResponseData seemingly gets called twice per request, once always empty, once with the
    // expected data.
    return absl::OkStatus();
  }
  data_called_++;
  if (config_.fail_data()) {
    if (data_called_ > config_.data_failure_countdown()) {
      return absl::InternalError("Intentional FakeUserDefinedOutputPlugin failure on data");
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Envoy::ProtobufWkt::Any> FakeUserDefinedOutputPlugin::getPerWorkerOutput() const {
  Envoy::Thread::LockGuard data_guard(data_lock_);
  Envoy::Thread::LockGuard headers_guard(headers_lock_);
  if (config_.fail_per_worker_output()) {
    return absl::InternalError(
        "Intentional FakeUserDefinedOutputPlugin failure on getting PerWorkerOutput");
  }
  FakeUserDefinedOutput output;
  output.set_data_called(data_called_);
  output.set_headers_called(headers_called_);
  output.set_worker_name(absl::StrCat("worker_", worker_metadata_.worker_number));

  Envoy::ProtobufWkt::Any output_any;
  output_any.PackFrom(output);
  return output_any;
}

std::string FakeUserDefinedOutputPluginFactory::name() const {
  return "nighthawk.fake_user_defined_output";
}
Envoy::ProtobufTypes::MessagePtr FakeUserDefinedOutputPluginFactory::createEmptyConfigProto() {
  return std::make_unique<FakeUserDefinedOutputConfig>();
}

absl::StatusOr<UserDefinedOutputPluginPtr>
FakeUserDefinedOutputPluginFactory::createUserDefinedOutputPlugin(
    const Envoy::ProtobufWkt::Any& message, const WorkerMetadata& worker_metadata) {
  plugin_count_++;
  FakeUserDefinedOutputConfig config;
  absl::Status status = Envoy::MessageUtil::unpackToNoThrow(message, config);
  if (!status.ok()) {
    return status;
  }
  return std::make_unique<FakeUserDefinedOutputPlugin>(config, worker_metadata);
}

absl::StatusOr<Envoy::ProtobufWkt::Any> FakeUserDefinedOutputPluginFactory::AggregateGlobalOutput(
    absl::Span<const nighthawk::client::UserDefinedOutput> per_worker_outputs) {
  FakeUserDefinedOutput global_output;
  global_output.set_worker_name("global");
  int data_called = 0;
  int headers_called = 0;
  for (const nighthawk::client::UserDefinedOutput& user_defined_output : per_worker_outputs) {
    if (user_defined_output.has_typed_output()) {
      Envoy::ProtobufWkt::Any any = user_defined_output.typed_output();
      FakeUserDefinedOutput output;
      absl::Status status = Envoy::MessageUtil::unpackToNoThrow(any, output);
      if (status.ok()) {
        data_called += output.data_called();
        headers_called += output.headers_called();
      } else {
        return status;
      }
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Cannot aggregate if any per_worker_outputs failed. Received: ",
                       user_defined_output.error_message()));
    }
  }
  global_output.set_data_called(data_called);
  global_output.set_headers_called(headers_called);

  Envoy::ProtobufWkt::Any global_any;
  global_any.PackFrom(global_output);
  return global_any;
}

int FakeUserDefinedOutputPluginFactory::getPluginCount() { return plugin_count_; }

REGISTER_FACTORY(FakeUserDefinedOutputPluginFactory, UserDefinedOutputPluginFactory);

} // namespace Nighthawk
