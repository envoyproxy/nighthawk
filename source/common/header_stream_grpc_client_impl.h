#pragma once

#include <queue>
#include <string>

#include "envoy/grpc/async_client.h"
#include "envoy/grpc/async_client_manager.h"
#include "envoy/stats/scope.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/common/header_source.h"
#include "nighthawk/common/header_stream_grpc_client.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/grpc/typed_async_client.h"
#include "external/envoy/source/common/http/header_map_impl.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/client/service.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Nighthawk {

class HeaderStreamGrpcClientImpl
    : public HeaderStreamGrpcClient,
      Envoy::Grpc::AsyncStreamCallbacks<nighthawk::client::HeaderStreamResponse>,
      Envoy::Logger::Loggable<Envoy::Logger::Id::upstream> {
public:
  HeaderStreamGrpcClientImpl(Envoy::Grpc::RawAsyncClientPtr async_client,
                             Envoy::Event::Dispatcher& dispatcher,
                             const Envoy::Http::HeaderMap& base_header,
                             const uint32_t header_buffer_length);

  // Grpc::AsyncStreamCallbacks
  void onCreateInitialMetadata(Envoy::Http::HeaderMap& metadata) override;
  void onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&& metadata) override;
  void
  onReceiveMessage(std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message) override;
  void onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&& metadata) override;
  void onRemoteClose(Envoy::Grpc::Status::GrpcStatus status, const std::string& message) override;
  HeaderMapPtr maybeDequeue() override;
  void start() override;
  bool stream_status_known() const override {
    return stream_ == nullptr || total_messages_received_ > 0;
  }

private:
  static const std::string METHOD_NAME;
  void trySendRequest();
  Envoy::Grpc::AsyncClient<nighthawk::client::HeaderStreamRequest,
                           nighthawk::client::HeaderStreamResponse>
      async_client_;
  Envoy::Grpc::AsyncStream<nighthawk::client::HeaderStreamRequest> stream_{};
  const Envoy::Protobuf::MethodDescriptor& service_method_;
  std::queue<std::unique_ptr<nighthawk::client::HeaderStreamResponse>> messages_;
  void emplaceMessage(std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message);
  uint32_t in_flight_headers_{0};
  uint32_t total_messages_received_{0};
  const Envoy::Http::HeaderMap& base_header_;
  const uint32_t header_buffer_length_;
};

} // namespace Nighthawk
