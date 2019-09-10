#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

//#include "envoy/config/bootstrap/v2/bootstrap.pb.h"
#include "envoy/grpc/async_client.h"
#include "envoy/grpc/async_client_manager.h"
#include "envoy/ratelimit/ratelimit.h"
#include "envoy/server/filter_config.h"
//#include "envoy/service/ratelimit/v2/rls.pb.h"
#include "envoy/stats/scope.h"
#include "envoy/tracing/http_tracer.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/common/logger.h"
#include "common/grpc/typed_async_client.h"
#include "common/singleton/const_singleton.h"

//#include "client/grpc/client.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/client/service.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Envoy {
namespace Upstream {

class GrpcControllerClient
    : Envoy::Grpc::AsyncStreamCallbacks<nighthawk::client::ExecutionResponse>,
      Envoy::Logger::Loggable<Envoy::Logger::Id::upstream> {
public:
  GrpcControllerClient(Envoy::Grpc::RawAsyncClientPtr async_client,
                       Envoy::Event::Dispatcher& dispatcher);

  // Grpc::AsyncStreamCallbacks
  void onCreateInitialMetadata(Envoy::Http::HeaderMap& metadata) override;
  void onReceiveInitialMetadata(Envoy::Http::HeaderMapPtr&& metadata) override;
  void onReceiveMessage(std::unique_ptr<nighthawk::client::ExecutionResponse>&& message) override;
  void onReceiveTrailingMetadata(Envoy::Http::HeaderMapPtr&& metadata) override;
  void onRemoteClose(Grpc::Status::GrpcStatus status, const std::string& message) override;

  // TODO(htuch): Make this configurable or some static.
  const uint32_t RETRY_DELAY_MS = 5000;

private:
  void setRetryTimer();
  void establishNewStream();
  void sendLoadStatsRequest();
  void handleFailure();
  void startLoadReportPeriod();

  // Envoy::Upstream::ClusterManager& cm_;
  Envoy::Grpc::AsyncClient<nighthawk::client::ExecutionRequest,
                           nighthawk::client::ExecutionResponse>
      async_client_;
  Envoy::Grpc::AsyncStream<nighthawk::client::ExecutionRequest> stream_{};
  const Protobuf::MethodDescriptor& service_method_;
  Event::TimerPtr retry_timer_;
  Event::TimerPtr response_timer_;
  nighthawk::client::ExecutionRequest request_;
  std::unique_ptr<nighthawk::client::ExecutionResponse> message_;
  // Map from cluster name to start of measurement interval.
  std::unordered_map<std::string, std::chrono::steady_clock::duration> clusters_;
  Envoy::TimeSource& time_source_;
};

using GrpcControllerClientPtr = std::unique_ptr<GrpcControllerClient>;

} // namespace Upstream
} // namespace Envoy
