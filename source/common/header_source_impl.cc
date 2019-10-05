#include "common/header_source_impl.h"

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

StaticHeaderSourceImpl::StaticHeaderSourceImpl(Envoy::Http::HeaderMapPtr&& header,
                                               const uint64_t max_yields)
    : header_(std::move(header)), yields_left_(max_yields) {
  RELEASE_ASSERT(header_ != nullptr, "header can't equal nullptr");
}

HeaderGenerator StaticHeaderSourceImpl::get() {
  return [this]() -> HeaderMapPtr {
    while (yields_left_--) {
      return header_;
    }
    return nullptr;
  };
}

RemoteHeaderSourceImpl::RemoteHeaderSourceImpl(Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                               Envoy::Event::Dispatcher& dispatcher,
                                               Envoy::Stats::Scope& scope,
                                               absl::string_view service_cluster_name,
                                               Envoy::Http::HeaderMapPtr&& base_header,
                                               uint32_t header_buffer_length)
    : cluster_manager_(cluster_manager), dispatcher_(dispatcher), scope_(scope),
      service_cluster_name_(std::string(service_cluster_name)),
      base_header_(std::move(base_header)), header_buffer_length_(header_buffer_length) {}

void RemoteHeaderSourceImpl::connectToHeaderStreamGrpcService() {
  auto clusters = cluster_manager_->clusters();
  RELEASE_ASSERT(clusters.find(service_cluster_name_) != clusters.end(),
                 "Source cluster not found");
  envoy::api::v2::core::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name(service_cluster_name_);
  auto cm =
      cluster_manager_->grpcAsyncClientManager().factoryForGrpcService(grpc_service, scope_, true);
  grpc_client_ = std::make_unique<HeaderStreamGrpcClientImpl>(cm->create(), dispatcher_,
                                                              *base_header_, header_buffer_length_);
  grpc_client_->start();
  while (!grpc_client_->stream_status_known()) {
    dispatcher_.run(Envoy::Event::Dispatcher::RunType::NonBlock);
  }
}

void RemoteHeaderSourceImpl::initOnThread() { connectToHeaderStreamGrpcService(); }

HeaderGenerator RemoteHeaderSourceImpl::get() {
  return [this]() -> HeaderMapPtr { return grpc_client_->maybeDequeue(); };
}

} // namespace Nighthawk