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
  response_timer_ = dispatcher.createTimer([this]() -> void { sendLoadStatsRequest(); });
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
  sendLoadStatsRequest();
}

void GrpcControllerClient::sendLoadStatsRequest() {
  ENVOY_LOG(trace, "Sending ExecutionRequest: {}", request_.DebugString());
  stream_->sendMessage(request_, false);
  // When the connection is established, the message has not yet been read so we
  // will not have a load reporting period.
  if (message_) {
    startLoadReportPeriod();
  }
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
  ENVOY_LOG(debug, "NighthawkService meessage received: {}", message->DebugString());
  message_ = std::move(message);
  startLoadReportPeriod();
}

void GrpcControllerClient::startLoadReportPeriod() {}

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
