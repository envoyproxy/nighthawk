#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/header_source.h"

#include "external/envoy/source/common/common/logger.h"

#include "common/header_stream_grpc_client_impl.h"

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

class RemoteHeaderSourceImpl : public BaseHeaderSourceImpl {
public:
  RemoteHeaderSourceImpl(Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                         Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                         absl::string_view service_cluster_name,
                         Envoy::Http::HeaderMapPtr&& base_header, uint32_t header_buffer_length);
  HeaderGenerator get() override;
  void initOnThread() override;

private:
  void connectToHeaderStreamGrpcService();
  Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::Scope& scope_;
  const std::string service_cluster_name_;
  HeaderStreamGrpcClientPtr grpc_client_;
  const HeaderMapPtr base_header_;
  const uint32_t header_buffer_length_;
};

} // namespace Nighthawk
