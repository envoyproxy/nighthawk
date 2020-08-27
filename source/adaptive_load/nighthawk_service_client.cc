#include "adaptive_load/nighthawk_service_client.h"

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

absl::StatusOr<nighthawk::client::ExecutionResponse> PerformNighthawkBenchmark(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const nighthawk::client::CommandLineOptions& command_line_options,
    const Envoy::Protobuf::Duration& duration) {
  nighthawk::client::CommandLineOptions options = command_line_options;
  *options.mutable_duration() = duration;

  nighthawk::client::ExecutionRequest request;
  nighthawk::client::ExecutionResponse response;
  *request.mutable_start_request()->mutable_options() = options;

  ::grpc::ClientContext context;
  std::shared_ptr<::grpc::ClientReaderWriterInterface<nighthawk::client::ExecutionRequest,
                                                      nighthawk::client::ExecutionResponse>>
      stream(nighthawk_service_stub->ExecutionStream(&context));

  if (!stream->Write(request)) {
    return absl::UnknownError("Failed to write request to the Nighthawk Service gRPC channel.");
  } else if (!stream->WritesDone()) {
    return absl::UnknownError("WritesDone() failed on the Nighthawk Service gRPC channel.");
  }

  bool got_response = false;
  while (stream->Read(&response)) {
    RELEASE_ASSERT(!got_response, "gRPC server has started sending more than one message.");
    got_response = true;
  }
  if (!got_response) {
    return absl::UnknownError("Nighthawk Service did not send a gRPC response.");
  }
  ::grpc::Status status = stream->Finish();
  if (!status.ok()) {
    return absl::Status(static_cast<absl::StatusCode>(status.error_code()), status.error_message());
  }
  return response;
}

} // namespace Nighthawk
