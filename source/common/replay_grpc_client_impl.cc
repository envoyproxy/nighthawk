#include "common/replay_grpc_client_impl.h"

#include <string>

#include "envoy/stats/scope.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/headers.h"

namespace Nighthawk {

ReplayGrpcClientImpl::ReplayGrpcClientImpl(Envoy::Grpc::RawAsyncClientPtr async_client,
                                           Envoy::Event::Dispatcher& dispatcher)
    : async_client_(std::move(async_client)),
      service_method_(*Envoy::Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          "nighthawk.client.NighthawkService.ExecutionStream")),
      dispatcher_(dispatcher) {
  establishNewStream();
  (void)(dispatcher_);
}

void ReplayGrpcClientImpl::establishNewStream() {
  ENVOY_LOG(debug, "Establishing new gRPC bidi stream for {}", service_method_.DebugString());
  stream_ = async_client_->start(service_method_, *this);
  if (stream_ == nullptr) {
    ENVOY_LOG(warn, "Unable to establish new stream");
    handleFailure();
    return;
  }
  nighthawk::client::ExecutionRequest request;
  auto options = request.mutable_start_request()->mutable_options();
  options->mutable_uri()->set_value("http://127.0.0.1:80/");
  options->mutable_duration()->set_seconds(1);
  options->mutable_requests_per_second()->set_value(1);
  options->mutable_verbosity()->set_value(nighthawk::client::Verbosity_VerbosityOptions_TRACE);
  sendRequest(request);
}

void ReplayGrpcClientImpl::sendRequest(const nighthawk::client::ExecutionRequest& request) {
  stream_->sendMessage(request, false);
  ENVOY_LOG(trace, "Sending ExecutionRequest: {}", request.DebugString());
}

void ReplayGrpcClientImpl::handleFailure() {
  ENVOY_LOG(critical, "NighthawkService stream/connection failure.");
}

void ReplayGrpcClientImpl::onCreateInitialMetadata(Envoy::Http::HeaderMap& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReplayGrpcClientImpl::onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReplayGrpcClientImpl::onReceiveMessage(
    std::unique_ptr<nighthawk::client::ExecutionResponse>&& message) {
  ENVOY_LOG(warn, "NighthawkService message received: {}", message->DebugString());
  // TODO(oschaaf): can we apply back pressure here?
  messages_.emplace(std::move(message));
}

void ReplayGrpcClientImpl::onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReplayGrpcClientImpl::onRemoteClose(Envoy::Grpc::Status::GrpcStatus status,
                                         const std::string& message) {
  ENVOY_LOG(warn, "gRPC stream closed: {}, {}", status, message);
  stream_ = nullptr;
  handleFailure();
}

} // namespace Nighthawk
