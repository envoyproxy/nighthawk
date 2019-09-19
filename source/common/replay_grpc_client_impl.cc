#include "common/replay_grpc_client_impl.h"

#include <string>

#include "envoy/stats/scope.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/headers.h"

namespace Nighthawk {

const std::string ReplayGrpcClientImpl::METHOD_NAME =
    "nighthawk.client.NighthawkService.HeaderStream";

const uint32_t ReplayGrpcClientImpl::QUEUE_LENGTH_WATERMARK = 50;

ReplayGrpcClientImpl::ReplayGrpcClientImpl(Envoy::Grpc::RawAsyncClientPtr async_client,
                                           Envoy::Event::Dispatcher&)
    : async_client_(std::move(async_client)),
      service_method_(
          *Envoy::Protobuf::DescriptorPool::generated_pool()->FindMethodByName(METHOD_NAME)) {}

bool ReplayGrpcClientImpl::establishNewStream() {
  stream_ = async_client_->start(service_method_, *this);
  const bool ok = stream_ != nullptr;
  ENVOY_LOG(trace, "stream establishment status ok: {}", ok);
  trySendRequest();
  return ok;
}

void ReplayGrpcClientImpl::trySendRequest() {
  if (stream_ != nullptr) {
    nighthawk::client::HeaderStreamRequest request;
    const uint32_t amount = QUEUE_LENGTH_WATERMARK;
    request.set_amount(amount);
    stream_->sendMessage(request, false);
    in_flight_headers_ = amount;
    ENVOY_LOG(trace, "send request: {}", request.DebugString());
  }
}

void ReplayGrpcClientImpl::onCreateInitialMetadata(Envoy::Http::HeaderMap&) {}

void ReplayGrpcClientImpl::onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&&) {}

void ReplayGrpcClientImpl::onReceiveMessage(
    std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message) {
  in_flight_headers_--;
  emplaceMessage(std::move(message));
}

void ReplayGrpcClientImpl::onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&&) {}

void ReplayGrpcClientImpl::onRemoteClose(Envoy::Grpc::Status::GrpcStatus status,
                                         const std::string& message) {
  ENVOY_LOG(trace, "remote close: {}, {}", status, message);
  stream_ = nullptr;
}

HeaderMapPtr ReplayGrpcClientImpl::maybeDequeue() {
  if (!messages_.empty()) {
    const auto& message = messages_.front();
    auto header = std::make_shared<Envoy::Http::HeaderMapImpl>();
    if (message->has_request_headers()) {
      const auto& message_request_headers = message->request_headers();
      for (const auto& message_header : message_request_headers.headers()) {
        header->addCopy(Envoy::Http::LowerCaseString(message_header.key()), message_header.value());
      }
    }
    messages_.pop();
    if (in_flight_headers_ == 0 && messages_.size() < QUEUE_LENGTH_WATERMARK) {
      trySendRequest();
    }
    return header;
  }
  return nullptr;
}

void ReplayGrpcClientImpl::emplaceMessage(
    std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message) {
  ENVOY_LOG(trace, "message received: {}", message->DebugString());
  messages_.emplace(std::move(message));
}

} // namespace Nighthawk
