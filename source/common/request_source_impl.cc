#include "common/request_source_impl.h"

#include <chrono>

#include "external/envoy/source/common/common/assert.h"

#include "common/request_impl.h"

namespace Nighthawk {

using namespace std::chrono_literals;

StaticRequestSourceImpl::StaticRequestSourceImpl(Envoy::Http::RequestHeaderMapPtr&& header,
                                                 const uint64_t max_yields)
    : header_(std::move(header)), yields_left_(max_yields) {
  RELEASE_ASSERT(header_ != nullptr, "header can't equal nullptr");
}

RequestGenerator StaticRequestSourceImpl::get() {
  return [this]() -> RequestPtr {
    while (yields_left_--) {
      return std::make_unique<RequestImpl>(header_);
    }
    return nullptr;
  };
}

RemoteRequestSourceImpl::RemoteRequestSourceImpl(
    const Envoy::Upstream::ClusterManagerPtr& cluster_manager, Envoy::Event::Dispatcher& dispatcher,
    Envoy::Stats::Scope& scope, absl::string_view service_cluster_name,
    Envoy::Http::RequestHeaderMapPtr&& base_header, uint32_t header_buffer_length)
    : cluster_manager_(cluster_manager), dispatcher_(dispatcher), scope_(scope),
      service_cluster_name_(std::string(service_cluster_name)),
      base_header_(std::move(base_header)), header_buffer_length_(header_buffer_length) {}

void RemoteRequestSourceImpl::connectToRequestStreamGrpcService() {
  Envoy::TimeSource& time_source = dispatcher_.timeSource();
  const auto clusters = cluster_manager_->clusters();
  const bool have_cluster =
      clusters.active_clusters_.find(service_cluster_name_) != clusters.active_clusters_.end();
  ASSERT(have_cluster);
  const std::chrono::seconds STREAM_SETUP_TIMEOUT = 60s;
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name(service_cluster_name_);
  Envoy::Grpc::AsyncClientFactoryPtr cluster_manager =
      cluster_manager_->grpcAsyncClientManager().factoryForGrpcService(
          grpc_service, scope_, Envoy::Grpc::AsyncClientFactoryClusterChecks::Skip);
  grpc_client_ = std::make_unique<RequestStreamGrpcClientImpl>(
      cluster_manager->create(), dispatcher_, *base_header_, header_buffer_length_);
  grpc_client_->start();
  const Envoy::MonotonicTime start = time_source.monotonicTime();
  bool timeout = false;
  // Wait for the client's initial stream setup to complete.
  do {
    dispatcher_.run(Envoy::Event::Dispatcher::RunType::NonBlock);
    timeout = (time_source.monotonicTime() - start) > STREAM_SETUP_TIMEOUT;
  } while (!grpc_client_->streamStatusKnown() && !timeout);
  ENVOY_LOG(debug, "Finished remote request source stream setup, connected: {}", timeout);
}

void RemoteRequestSourceImpl::initOnThread() { connectToRequestStreamGrpcService(); }

RequestGenerator RemoteRequestSourceImpl::get() {
  return [this]() -> RequestPtr { return grpc_client_->maybeDequeue(); };
}

} // namespace Nighthawk