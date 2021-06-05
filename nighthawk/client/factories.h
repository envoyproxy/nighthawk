#pragma once

#include <memory>

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_formatter.h"
#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/termination_predicate.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/envoy/api/api.h"
#include "external/envoy/envoy/common/pure.h"
#include "external/envoy/envoy/common/time.h"
#include "external/envoy/envoy/event/dispatcher.h"
#include "external/envoy/envoy/upstream/cluster_manager.h"

namespace Nighthawk {
namespace Client {

class BenchmarkClientFactory {
public:
  virtual ~BenchmarkClientFactory() = default;

  /**
   * Constructs a BenchmarkClient
   *
   * @param api reference to the Api object.
   * @param dispatcher supplies the owning thread's dispatcher.
   * @param scope stats scope for any stats tracked by the benchmark client.
   * @param cluster_manager Cluster manager preconfigured with our target cluster.
   * @param http_tracer Shared pointer to an http tracer implementation (e.g. Zipkin).
   * @param cluster_name Name of the cluster that this benchmark client
   * will use. In conjunction with cluster_manager this will allow the this BenchmarkClient to
   * access the target connection pool.
   * @param worker_id Worker number.
   * @param request_source Source of request-specifiers. Will be queries every time the
   * BenchmarkClient is asked to issue a request.
   *
   * @return BenchmarkClientPtr pointer to a BenchmarkClient instance.
   */
  virtual BenchmarkClientPtr create(Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher,
                                    Envoy::Stats::Scope& scope,
                                    Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                    Envoy::Tracing::HttpTracerSharedPtr& http_tracer,
                                    absl::string_view cluster_name, int worker_id,
                                    RequestSource& request_source) const PURE;
};

class OutputFormatterFactory {
public:
  virtual ~OutputFormatterFactory() = default;

  /**
   * Constructs an OutputFormatter instance according to the requested output format.
   *
   * @param options Proto configuration object indicating the desired output format.
   *
   * @return OutputFormatterPtr pointer to an OutputFormatter instance.
   */
  virtual OutputFormatterPtr
  create(const nighthawk::client::OutputFormat_OutputFormatOptions options) const PURE;
};

} // namespace Client
} // namespace Nighthawk
