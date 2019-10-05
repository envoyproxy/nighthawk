#include "common/header_stream_grpc_client_impl.h"

#include <string>

#include "envoy/stats/scope.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/headers.h"

namespace Nighthawk {

const std::string HeaderStreamGrpcClientImpl::METHOD_NAME =
    "nighthawk.client.NighthawkService.HeaderStream";

HeaderStreamGrpcClientImpl::HeaderStreamGrpcClientImpl(Envoy::Grpc::RawAsyncClientPtr async_client,
                                                       Envoy::Event::Dispatcher&,
                                                       const Envoy::Http::HeaderMap& base_header,
                                                       const uint32_t header_buffer_length)
    : async_client_(std::move(async_client)),
      service_method_(
          *Envoy::Protobuf::DescriptorPool::generated_pool()->FindMethodByName(METHOD_NAME)),
      base_header_(base_header), header_buffer_length_(header_buffer_length) {}

void HeaderStreamGrpcClientImpl::start() {
  stream_ = async_client_->start(service_method_, *this);
  ENVOY_LOG(trace, "stream establishment status ok: {}", stream_ != nullptr);
  trySendRequest();
}

void HeaderStreamGrpcClientImpl::trySendRequest() {
  if (stream_ != nullptr) {
    nighthawk::client::HeaderStreamRequest request;
    request.set_amount(header_buffer_length_);
    stream_->sendMessage(request, false);
    in_flight_headers_ = header_buffer_length_;
    ENVOY_LOG(trace, "send request: {}", request.DebugString());
  }
}

void HeaderStreamGrpcClientImpl::onCreateInitialMetadata(Envoy::Http::HeaderMap&) {}

void HeaderStreamGrpcClientImpl::onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&&) {}

void HeaderStreamGrpcClientImpl::onReceiveMessage(
    std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message) {
  in_flight_headers_--;
  total_messages_received_++;
  emplaceMessage(std::move(message));
}

void HeaderStreamGrpcClientImpl::onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&&) {}

void HeaderStreamGrpcClientImpl::onRemoteClose(Envoy::Grpc::Status::GrpcStatus status,
                                               const std::string& message) {
  const std::string log_message =
      fmt::format("Remote close. Status: {}, Message: '{}', amount of in-flight headers {}, "
                  "total messages received: {}",
                  status, message, in_flight_headers_, total_messages_received_);
  if (in_flight_headers_ || total_messages_received_ == 0) {
    ENVOY_LOG(error, "{}", log_message);
  } else {
    ENVOY_LOG(trace, "{}", log_message);
  }
  stream_ = nullptr;
}

HeaderMapPtr HeaderStreamGrpcClientImpl::maybeDequeue() {
  if (!messages_.empty()) {
    const auto& message = messages_.front();
    auto header = std::make_shared<Envoy::Http::HeaderMapImpl>(base_header_);
    if (message->has_request_headers()) {
      const auto& message_request_headers = message->request_headers();
      for (const auto& message_header : message_request_headers.headers()) {
        header->addCopy(Envoy::Http::LowerCaseString(message_header.key()), message_header.value());
      }
    }
    messages_.pop();
    if (in_flight_headers_ == 0 && messages_.size() < header_buffer_length_) {
      trySendRequest();
    }
    return header;
  }
  return nullptr;
}

void HeaderStreamGrpcClientImpl::emplaceMessage(
    std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message) {
  ENVOY_LOG(trace, "message received: {}", message->DebugString());
  messages_.emplace(std::move(message));
}

} // namespace Nighthawk
