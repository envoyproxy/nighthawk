#include "client/grpc/client_impl.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

//#include "envoy/api/v2/ratelimit/ratelimit.pb.h"
#include "envoy/stats/scope.h"

#include "common/common/assert.h"
#include "common/http/header_map_impl.h"
#include "common/http/headers.h"

namespace Envoy {
namespace Upstream {

GrpcControllerClient::GrpcControllerClient(Envoy::Grpc::RawAsyncClientPtr async_client,
                                           Envoy::Event::Dispatcher& dispatcher)
    : async_client_(std::move(async_client)),
      service_method_(*Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          "nighthawk.client.NighthawkService.ExecutionStream")),
      time_source_(dispatcher.timeSource()) {
  retry_timer_ = dispatcher.createTimer([this]() -> void { establishNewStream(); });
  response_timer_ = dispatcher.createTimer([this]() -> void { subscribe(); });
  establishNewStream();
}

void GrpcControllerClient::setRetryTimer() {
  retry_timer_->enableTimer(std::chrono::milliseconds(RETRY_DELAY_MS));
}

void GrpcControllerClient::establishNewStream() {
  ENVOY_LOG(debug, "Establishing new gRPC bidi stream for {}", service_method_.DebugString());
  stream_ = async_client_->start(service_method_, *this);
  if (stream_ == nullptr) {
    ENVOY_LOG(warn, "Unable to establish new stream");
    handleFailure();
    return;
  }
  subscribe();
}

void GrpcControllerClient::subscribe() {
  auto options = request_.mutable_start_request()->mutable_options();
  options->mutable_uri()->set_value("http://127.0.0.1:80/");
  options->mutable_duration()->set_seconds(1);
  options->mutable_requests_per_second()->set_value(1);
  options->mutable_verbosity()->set_value(nighthawk::client::Verbosity_VerbosityOptions_TRACE);
  stream_->sendMessage(request_, false);
  ENVOY_LOG(trace, "Sending ExecutionRequest: {}", request_.DebugString());
}

void GrpcControllerClient::handleFailure() {
  ENVOY_LOG(warn, "NighthawkService stream/connection failure, will retry in {} ms.",
            RETRY_DELAY_MS);
  setRetryTimer();
}

void GrpcControllerClient::onCreateInitialMetadata(Http::HeaderMap& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void GrpcControllerClient::onReceiveInitialMetadata(Http::HeaderMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void GrpcControllerClient::onReceiveMessage(
    std::unique_ptr<nighthawk::client::ExecutionResponse>&& message) {
  ENVOY_LOG(warn, "NighthawkService message received: {}", message->DebugString());
  message_ = std::move(message);
}

void GrpcControllerClient::onReceiveTrailingMetadata(Http::HeaderMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void GrpcControllerClient::onRemoteClose(Grpc::Status::GrpcStatus status,
                                         const std::string& message) {
  ENVOY_LOG(warn, "gRPC config stream closed: {}, {}", status, message);
  response_timer_->disableTimer();
  stream_ = nullptr;
  handleFailure();
}

} // namespace Upstream
} // namespace Envoy
