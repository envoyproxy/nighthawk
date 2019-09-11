#pragma once

#include "envoy/http/header_map.h"

#include "external/envoy/source/common/common/logger.h"

#include "nighthawk/common/header_source.h"

#include "common/replay_grpc_client_impl.h"

namespace Nighthawk {

class BaseHeaderSourceImpl : public HeaderSource,
                             public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
  void initOnThread() override{};
};

class StaticHeaderSourceImpl : public BaseHeaderSourceImpl {
public:
  StaticHeaderSourceImpl(Envoy::Http::HeaderMapPtr&&, const uint64_t max_yields = UINT64_MAX);
  HeaderGenerator get() override;

private:
  const HeaderMapPtr header_;
  uint64_t yields_left_;
};

class ReplayHeaderSourceImpl : public BaseHeaderSourceImpl {
public:
  ReplayHeaderSourceImpl(Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                         Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                         absl::string_view service_cluster_name);
  HeaderGenerator get() override;
  void initOnThread() override { connectToReplayGrpcSourceService(); };

private:
  void connectToReplayGrpcSourceService();
  Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::Scope& scope_;
  const std::string service_cluster_name_;
  ReplayGrpcClientImplPtr grpc_client_;
};

} // namespace Nighthawk
