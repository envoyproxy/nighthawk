#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"
#include "nighthawk/common/request_source.h"

#include "external/envoy/source/common/common/logger.h"

#include "source/common/request_stream_grpc_client_impl.h"

namespace Nighthawk {

class BaseRequestSourceImpl : public RequestSource,
                              public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {};

/**
 * Implements a static request source, which yields the same request specifier over and over.
 */
class StaticRequestSourceImpl : public BaseRequestSourceImpl {
public:
  /**
   * @param max_yields the number of request specifiers to yield. The source will start yielding
   * nullptr when exceeded.
   */
  StaticRequestSourceImpl(Envoy::Http::RequestHeaderMapPtr&&,
                          const uint64_t max_yields = UINT64_MAX);
  RequestGenerator get() override;
  void initOnThread() override{};

private:
  const HeaderMapPtr header_;
  uint64_t yields_left_;
};

/**
 * Remote request source implementation. Will connect to a gRPC service to pull request specifiers,
 * and yield results based on that.
 */
class RemoteRequestSourceImpl : public BaseRequestSourceImpl {
public:
  /**
   * @param cluster_manager Cluster manager preconfigured with our target cluster.
   * @param dispatcher Dispatcher that will be used.
   * @param scope Statistics scope that will be used.
   * @param service_cluster_name The name of the cluster that should be used to connect.
   * @param base_header Any headers in request specifiers yielded by the remote request
   * source service will override what is specified here.
   * @param header_buffer_length The number of messages to buffer.
   */
  RemoteRequestSourceImpl(const Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                          Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                          absl::string_view service_cluster_name,
                          Envoy::Http::RequestHeaderMapPtr&& base_header,
                          uint32_t header_buffer_length);
  RequestGenerator get() override;
  void initOnThread() override;

private:
  void connectToRequestStreamGrpcService();
  const Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::Scope& scope_;
  const std::string service_cluster_name_;
  RequestStreamGrpcClientPtr grpc_client_;
  const HeaderMapPtr base_header_;
  const uint32_t header_buffer_length_;
};

} // namespace Nighthawk
