#include "distributor/service_impl.h"

#include <grpc++/grpc++.h>

#include "envoy/config/core/v3/base.pb.h"

#include "external/envoy/source/common/common/assert.h"

#include "common/nighthawk_service_client_impl.h"

#include "sink/nighthawk_sink_client_impl.h"

namespace Nighthawk {

::grpc::Status NighthawkDistributorServiceImpl::validateRequest(
    const ::nighthawk::DistributedRequest& request) const {
  // xxx: why the std::strings() below?
  if (request.has_execution_request()) {
    if (request.services_size() == 0) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "DistributedRequest.ExecutionRequest MUST specify one or more services.");
    }
    const ::nighthawk::client::ExecutionRequest& execution_request = request.execution_request();
    if (!execution_request.has_start_request()) {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "DistributedRequest.ExecutionRequest MUST have StartRequest.");
    }
    if (!execution_request.start_request().has_options()) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          "DistributedRequest.ExecutionRequest.StartRequest MUST have CommandLineOptions.");
    }
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "DistributedRequest.ExecutionRequest MUST be specified.");
  }
  return ::grpc::Status::OK;
}

absl::StatusOr<nighthawk::client::ExecutionResponse>
NighthawkDistributorServiceImpl::handleExecutionRequest(
    const envoy::config::core::v3::Address& service,
    const ::nighthawk::client::ExecutionRequest& request) const {
  NighthawkServiceClientImpl client;
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub;
  std::shared_ptr<::grpc::Channel> channel;
  channel = grpc::CreateChannel(fmt::format("{}:{}", service.socket_address().address(),
                                            service.socket_address().port_value()),
                                grpc::InsecureChannelCredentials());
  stub = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel);
  return client.PerformNighthawkBenchmark(stub.get(), request.start_request().options());
}

// Translates one or more backend response into a single reply message
std::tuple<grpc::Status, nighthawk::DistributedResponse>
NighthawkDistributorServiceImpl::handleRequest(
    const ::nighthawk::DistributedRequest& request) const {
  RELEASE_ASSERT(request.has_execution_request(), "request.has_execution_request()");
  RELEASE_ASSERT(request.services_size() != 0, "services_size() == 0");
  RELEASE_ASSERT(request.execution_request().has_start_request(), "no start_request");
  RELEASE_ASSERT(request.execution_request().start_request().has_options(), "no options");
  ENVOY_LOG(trace, "Handling execution request");

  nighthawk::DistributedResponse response;
  bool has_errors = false;
  for (const envoy::config::core::v3::Address& service : request.services()) {
    absl::StatusOr<nighthawk::client::ExecutionResponse> execution_response =
        handleExecutionRequest(service, request.execution_request());
    nighthawk::DistributedResponseFragment* response_fragment = response.add_fragment();
    response_fragment->mutable_service()->MergeFrom(service);
    if (execution_response.ok()) {
      *response_fragment->mutable_execution_response() = execution_response.value();
    } else {
      response_fragment->mutable_error()->set_code(
          static_cast<int>(execution_response.status().code()));
      response_fragment->mutable_error()->set_message(
          std::string("Distributed Execution Request failed: ") +
          std::string(execution_response.status().message()));
      has_errors = true;
    }
  }
  return {has_errors
              ? grpc::Status(grpc::StatusCode::INTERNAL, "One or more execution requests failed")
              : grpc::Status::OK,
          response};
}

::grpc::Status NighthawkDistributorServiceImpl::DistributedRequestStream(
    ::grpc::ServerContext*,
    ::grpc::ServerReaderWriter<::nighthawk::DistributedResponse, ::nighthawk::DistributedRequest>*
        stream) {
  nighthawk::DistributedRequest request;
  ::grpc::Status status = grpc::Status::OK;
  while (status.ok() && stream->Read(&request)) {
    ENVOY_LOG(trace, "Inbound DistributedRequest {}", request.DebugString());
    status = validateRequest(request);
    if (status.ok()) {
      std::tuple<grpc::Status, nighthawk::DistributedResponse> status_and_response =
          handleRequest(request);
      status = std::get<0>(status_and_response);
      nighthawk::DistributedResponse response = std::get<1>(status_and_response);
      if (!stream->Write(response)) {
        ENVOY_LOG(error, "Failed to write DistributedResponse.");
        status = grpc::Status(grpc::StatusCode::INTERNAL,
                              std::string("Failed to write DistributedResponse."));
      } else {
        ENVOY_LOG(trace, "Wrote DistributedResponse {}", response.DebugString());
      }
    } else {
      ENVOY_LOG(error, "DistributedRequest invalid: ({}) '{}'", status.error_code(),
                status.error_message());
    }
  }
  ENVOY_LOG(trace, "Finishing stream with status {}:{}", status.error_code(),
            status.error_message());
  return status;
}

} // namespace Nighthawk
