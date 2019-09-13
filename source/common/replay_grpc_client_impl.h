#pragma once

#include <queue>
#include <string>

#include "envoy/grpc/async_client.h"
#include "envoy/grpc/async_client_manager.h"
#include "envoy/stats/scope.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/common/header_source.h"
#include "nighthawk/common/replay_grpc_client.h"

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

class ReplayGrpcClientImpl
    : public ReplayGrpcClient,
      Envoy::Grpc::AsyncStreamCallbacks<nighthawk::client::HeaderStreamResponse>,
      Envoy::Logger::Loggable<Envoy::Logger::Id::upstream> {
public:
  ReplayGrpcClientImpl(Envoy::Grpc::RawAsyncClientPtr async_client,
                       Envoy::Event::Dispatcher& dispatcher);

  // Grpc::AsyncStreamCallbacks
  void onCreateInitialMetadata(Envoy::Http::HeaderMap& metadata) override;
  void onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&& metadata) override;
  void
  onReceiveMessage(std::unique_ptr<nighthawk::client::HeaderStreamResponse>&& message) override;
  void onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&& metadata) override;
  void onRemoteClose(Envoy::Grpc::Status::GrpcStatus status, const std::string& message) override;
  HeaderMapPtr maybeDequeue() override;

private:
  void sendRequest(const nighthawk::client::HeaderStreamRequest& request);
  void establishNewStream();
  void handleFailure();

  Envoy::Grpc::AsyncClient<nighthawk::client::HeaderStreamRequest,
                           nighthawk::client::HeaderStreamResponse>
      async_client_;
  Envoy::Grpc::AsyncStream<nighthawk::client::HeaderStreamRequest> stream_{};
  const Envoy::Protobuf::MethodDescriptor& service_method_;
  std::queue<std::unique_ptr<nighthawk::client::HeaderStreamResponse>> messages_;
  Envoy::Event::Dispatcher& dispatcher_;
};

} // namespace Nighthawk
