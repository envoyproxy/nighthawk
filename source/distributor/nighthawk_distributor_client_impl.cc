#include "distributor/nighthawk_distributor_client_impl.h"

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

absl::StatusOr<nighthawk::DistributedResponse> NighthawkDistributorClientImpl::DistributedRequest(
    nighthawk::NighthawkDistributor::StubInterface& nighthawk_distributor_stub,
    const nighthawk::DistributedRequest& distributed_request) const {
  ::grpc::ClientContext context;
  std::shared_ptr<::grpc::ClientReaderWriterInterface<nighthawk::DistributedRequest,
                                                      nighthawk::DistributedResponse>>
      stream(nighthawk_distributor_stub.DistributedRequestStream(&context));
  ENVOY_LOG_MISC(trace, "Write {}", distributed_request.DebugString());
  if (!stream->Write(distributed_request)) {
    return absl::UnavailableError(
        "Failed to write request to the Nighthawk Distributor gRPC channel.");
  } else if (!stream->WritesDone()) {
    return absl::InternalError("WritesDone() failed on the Nighthawk Distributor gRPC channel.");
  }

  bool got_response = false;
  nighthawk::DistributedResponse response;
  while (stream->Read(&response)) {
    RELEASE_ASSERT(!got_response,
                   "Distributor Service has started responding with more than one message.");
    got_response = true;
    ENVOY_LOG_MISC(trace, "Read {}", response.DebugString());
  }
  if (!got_response) {
    return absl::InternalError("Distributor Service did not send a gRPC response.");
  }
  ::grpc::Status status = stream->Finish();
  ENVOY_LOG_MISC(trace, "Finish {}", status.ok());
  if (!status.ok()) {
    return absl::Status(static_cast<absl::StatusCode>(status.error_code()), status.error_message());
  }
  return response;
}

} // namespace Nighthawk
