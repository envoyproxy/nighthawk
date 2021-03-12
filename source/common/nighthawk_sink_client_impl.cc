#include "common/nighthawk_sink_client_impl.h"

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

absl::StatusOr<nighthawk::sink::StoreExecutionResponse>
NighthawkSinkClientImpl::StoreExecutionResponseStream(
    nighthawk::sink::NighthawkSink::StubInterface* nighthawk_sink_stub,
    const nighthawk::sink::StoreExecutionRequest& store_execution_request) const {
  ::grpc::ClientContext context;
  ::nighthawk::sink::StoreExecutionResponse store_execution_response;
  std::shared_ptr<::grpc::ClientWriterInterface<::nighthawk::sink::StoreExecutionRequest>> stream(
      nighthawk_sink_stub->StoreExecutionResponseStream(&context, &store_execution_response));
  if (!stream->Write(store_execution_request)) {
    return absl::UnavailableError("Failed to write request to the Nighthawk Sink gRPC channel.");
  } else if (!stream->WritesDone()) {
    return absl::InternalError("WritesDone() failed on the Nighthawk Sink gRPC channel.");
  }
  ::grpc::Status status = stream->Finish();
  if (!status.ok()) {
    return absl::Status(static_cast<absl::StatusCode>(status.error_code()), status.error_message());
  }
  return store_execution_response;
}

absl::StatusOr<nighthawk::sink::SinkResponse> NighthawkSinkClientImpl::SinkRequestStream(
    nighthawk::sink::NighthawkSink::StubInterface& nighthawk_sink_stub,
    const nighthawk::sink::SinkRequest& sink_request) const {
  nighthawk::sink::SinkResponse response;

  ::grpc::ClientContext context;
  std::shared_ptr<::grpc::ClientReaderWriterInterface<nighthawk::sink::SinkRequest,
                                                      nighthawk::sink::SinkResponse>>
      stream(nighthawk_sink_stub.SinkRequestStream(&context));

  if (!stream->Write(sink_request)) {
    return absl::UnavailableError("Failed to write request to the Nighthawk Sink gRPC channel.");
  } else if (!stream->WritesDone()) {
    return absl::InternalError("WritesDone() failed on the Nighthawk Sink gRPC channel.");
  }

  bool got_response = false;
  while (stream->Read(&response)) {
    RELEASE_ASSERT(!got_response,
                   "Sink Service has started responding with more than one message.");
    got_response = true;
  }
  ::grpc::Status status = stream->Finish();
  if (!status.ok()) {
    return absl::Status(static_cast<absl::StatusCode>(status.error_code()), status.error_message());
  }
  return response;
}

} // namespace Nighthawk
