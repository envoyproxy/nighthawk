#include "common/request_stream_grpc_client_impl.h"

#include <string>

#include "envoy/stats/scope.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/headers.h"

#include "common/request_impl.h"

namespace Nighthawk {

const std::string RequestStreamGrpcClientImpl::METHOD_NAME =
    "nighthawk.client.NighthawkService.RequestStream";

RequestStreamGrpcClientImpl::RequestStreamGrpcClientImpl(
    Envoy::Grpc::RawAsyncClientPtr async_client, Envoy::Event::Dispatcher&,
    const Envoy::Http::HeaderMap& base_header, const uint32_t header_buffer_length)
    : async_client_(std::move(async_client)),
      service_method_(
          *Envoy::Protobuf::DescriptorPool::generated_pool()->FindMethodByName(METHOD_NAME)),
      base_header_(base_header), header_buffer_length_(header_buffer_length) {}

void RequestStreamGrpcClientImpl::start() {
  stream_ = async_client_->start(service_method_, *this, Envoy::Http::AsyncClient::StreamOptions());
  ENVOY_LOG(trace, "stream establishment status ok: {}", stream_ != nullptr);
  trySendRequest();
}

void RequestStreamGrpcClientImpl::trySendRequest() {
  if (stream_ != nullptr) {
    nighthawk::client::RequestStreamRequest request;
    request.set_amount(header_buffer_length_);
    stream_->sendMessage(request, false);
    in_flight_headers_ = header_buffer_length_;
    ENVOY_LOG(trace, "send request: {}", request.DebugString());
  }
}

void RequestStreamGrpcClientImpl::onCreateInitialMetadata(Envoy::Http::HeaderMap&) {}

void RequestStreamGrpcClientImpl::onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&&) {}

void RequestStreamGrpcClientImpl::onReceiveMessage(
    std::unique_ptr<nighthawk::client::RequestStreamResponse>&& message) {
  in_flight_headers_--;
  total_messages_received_++;
  emplaceMessage(std::move(message));
}

void RequestStreamGrpcClientImpl::onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&&) {}

void RequestStreamGrpcClientImpl::onRemoteClose(Envoy::Grpc::Status::GrpcStatus status,
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

RequestPtr RequestStreamGrpcClientImpl::messageToRequest(
    const nighthawk::client::RequestStreamResponse&) const {
  auto header = std::make_shared<Envoy::Http::HeaderMapImpl>(base_header_);
  RequestPtr request = std::make_unique<RequestImpl>(header);

  // TODO(oschaaf): extract a method which handles the request specifier based
  // on the commented code once we have test coverage.
  /*
    if (message.has_request_specifier()) {
      const auto& request_specifier = message.request_specifier();
      if (request_specifier.has_headers()) {
        const auto& message_request_headers = request_specifier.headers();
        for (const auto& message_header : message_request_headers.headers()) {
          header->addCopy(Envoy::Http::LowerCaseString(message_header.key()),
    message_header.value());
        }
      }
      if (request_specifier.has_content_length()) {
        std::string s_content_length = absl::StrCat("", request_specifier.content_length().value());
        header->insertContentLength().value(s_content_length);
      }
      if (request_specifier.has_authority()) {
        header->insertHost().value(request_specifier.authority().value());
      }
      if (request_specifier.has_uri()) {
        header->insertPath().value(request_specifier.uri().value());
      }
      if (request_specifier.has_method()) {
        header->insertMethod().value(request_specifier.method().value());
      }
    }

  */
  // TODO(oschaaf): associate the expectations from the proto to the request,
  // and process those by verifying expectations on request completion.
  return request;
}

RequestPtr RequestStreamGrpcClientImpl::maybeDequeue() {
  RequestPtr request = nullptr;
  if (!messages_.empty()) {
    const auto& message = messages_.front();
    request = messageToRequest(*message);
    messages_.pop();
    if (in_flight_headers_ == 0 && messages_.size() < header_buffer_length_) {
      trySendRequest();
    }
  }

  return request;
}

void RequestStreamGrpcClientImpl::emplaceMessage(
    std::unique_ptr<nighthawk::client::RequestStreamResponse>&& message) {
  ENVOY_LOG(trace, "message received: {}", message->DebugString());
  messages_.emplace(std::move(message));
}

} // namespace Nighthawk
