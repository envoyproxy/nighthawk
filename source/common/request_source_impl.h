#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"
#include "nighthawk/common/request_source.h"

#include "external/envoy/source/common/common/logger.h"

#include "common/request_stream_grpc_client_impl.h"

namespace Nighthawk {

class BaseRequestSourceImpl : public RequestSource,
                              public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
  void initOnThread() override{};
};

class StaticRequestSourceImpl : public BaseRequestSourceImpl {
public:
  StaticRequestSourceImpl(Envoy::Http::HeaderMapPtr&&, const uint64_t max_yields = UINT64_MAX);
  RequestGenerator get() override;

private:
  const HeaderMapPtr header_;
  uint64_t yields_left_;
};

class RemoteRequestSourceImpl : public BaseRequestSourceImpl {
public:
  RemoteRequestSourceImpl(Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                          Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                          absl::string_view service_cluster_name,
                          Envoy::Http::HeaderMapPtr&& base_header, uint32_t header_buffer_length);
  RequestGenerator get() override;
  void initOnThread() override;

private:
  void connectToRequestStreamGrpcService();
  Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::Scope& scope_;
  const std::string service_cluster_name_;
  RequestStreamGrpcClientPtr grpc_client_;
  const HeaderMapPtr base_header_;
  const uint32_t header_buffer_length_;
};

} // namespace Nighthawk
