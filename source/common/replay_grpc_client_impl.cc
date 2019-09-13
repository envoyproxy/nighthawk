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
          "nighthawk.client.NighthawkService.HeaderStream")),
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
  nighthawk::client::HeaderStreamRequest request;
  sendRequest(request);
}

void ReplayGrpcClientImpl::sendRequest(const nighthawk::client::HeaderStreamRequest& request) {
  stream_->sendMessage(request, false);
  ENVOY_LOG(trace, "Sending HeaderStreamRequest: {}", request.DebugString());
}

void ReplayGrpcClientImpl::handleFailure() {
  ENVOY_LOG(error, "NighthawkService stream/connection failure.");
}

void ReplayGrpcClientImpl::onCreateInitialMetadata(Envoy::Http::HeaderMap& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReplayGrpcClientImpl::onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReplayGrpcClientImpl::onReceiveMessage(
    std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message) {
  ENVOY_LOG(trace, "NighthawkService message received: {}", message->DebugString());
  // TODO(oschaaf): flow control.
  messages_.emplace(std::move(message));
}

void ReplayGrpcClientImpl::onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&& metadata) {
  UNREFERENCED_PARAMETER(metadata);
}

void ReplayGrpcClientImpl::onRemoteClose(Envoy::Grpc::Status::GrpcStatus status,
                                         const std::string& message) {
  ENVOY_LOG(info, "gRPC header stream remote close: {}, {}", status, message);
  stream_ = nullptr;
}

HeaderMapPtr ReplayGrpcClientImpl::maybeDequeue() {
  if (!messages_.empty()) {
    // XXX(oschaaf): fix potential null dereferences.
    const auto& message = messages_.front();
    auto header = std::make_shared<Envoy::Http::HeaderMapImpl>();
    const auto& message_request_headers = message->request_headers();
    for (const auto& message_header : message_request_headers.headers()) {
      header->addCopy(Envoy::Http::LowerCaseString(message_header.key()), message_header.value());
    }
    messages_.pop();
    return header;
  }
  return nullptr;
}

} // namespace Nighthawk
