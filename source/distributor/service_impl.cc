#include "distributor/service_impl.h"

#include <grpc++/grpc++.h>

#include "envoy/config/core/v3/base.pb.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/distributor/distributor.pb.validate.h"

namespace Nighthawk {
namespace {

grpc::Status validateRequest(const nighthawk::DistributedRequest& request) {
  Envoy::ProtobufMessage::ValidationVisitor& validation_visitor =
      Envoy::ProtobufMessage::getStrictValidationVisitor();
  try {
    Envoy::MessageUtil::validate(request, validation_visitor);
  } catch (const Envoy::ProtoValidationException& e) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what());
  }

  if (request.has_execution_request()) {
    const nighthawk::client::ExecutionRequest& execution_request = request.execution_request();
    if (!execution_request.start_request().has_options()) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          "DistributedRequest.ExecutionRequest.StartRequest MUST have CommandLineOptions.");
    }
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "DistributedRequest.ExecutionRequest MUST be specified.");
  }
  return grpc::Status::OK;
}

} // namespace

absl::StatusOr<nighthawk::client::ExecutionResponse>
NighthawkDistributorServiceImpl::handleExecutionRequest(
    const envoy::config::core::v3::Address& service,
    const nighthawk::client::ExecutionRequest& request) const {
  RELEASE_ASSERT(service_client_ != nullptr, "service_client_ != nullptr");
  std::shared_ptr<grpc::Channel> channel;
  channel = grpc::CreateChannel(fmt::format("{}:{}", service.socket_address().address(),
                                            service.socket_address().port_value()),
                                grpc::InsecureChannelCredentials());
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub =
      std::make_unique<nighthawk::client::NighthawkService::Stub>(channel);
  return service_client_->PerformNighthawkBenchmark(stub.get(), request.start_request().options());
}

// Translates one or more backend response into a single reply message
std::tuple<grpc::Status, nighthawk::DistributedResponse>
NighthawkDistributorServiceImpl::handleRequest(const nighthawk::DistributedRequest& request) const {
  ENVOY_LOG(trace, "Handling execution request");
  nighthawk::DistributedResponse response;
  bool has_errors = false;
  for (const envoy::config::core::v3::Address& service : request.services()) {
    absl::StatusOr<nighthawk::client::ExecutionResponse> execution_response =
        handleExecutionRequest(service, request.execution_request());
    nighthawk::DistributedServiceResponse* service_response = response.add_service_response();
    service_response->mutable_service()->MergeFrom(service);
    if (execution_response.ok()) {
      *service_response->mutable_execution_response() = execution_response.value();
    } else {
      service_response->mutable_error()->set_code(
          static_cast<int>(execution_response.status().code()));
      service_response->mutable_error()->set_message(
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

grpc::Status NighthawkDistributorServiceImpl::DistributedRequestStream(
    grpc::ServerContext*,
    grpc::ServerReaderWriter<nighthawk::DistributedResponse, nighthawk::DistributedRequest>*
        stream) {
  nighthawk::DistributedRequest request;
  grpc::Status status = grpc::Status::OK;
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
