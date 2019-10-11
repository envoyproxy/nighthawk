#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/factories.h"
#include "nighthawk/common/uri.h"

#include "common/platform_util_impl.h"

namespace Nighthawk {
namespace Client {

class OptionBasedFactoryImpl {
public:
  OptionBasedFactoryImpl(const Options& options);
  virtual ~OptionBasedFactoryImpl() = default;

protected:
  const Options& options_;
  const PlatformUtilImpl platform_util_;
};

class BenchmarkClientFactoryImpl : public OptionBasedFactoryImpl, public BenchmarkClientFactory {
public:
  BenchmarkClientFactoryImpl(const Options& options);
  BenchmarkClientPtr create(Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher,
                            Envoy::Stats::Scope& scope,
                            Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                            Envoy::Tracing::HttpTracerPtr& http_tracer,
                            absl::string_view cluster_name,
                            RequestSource& request_generator) const override;
};

class SequencerFactoryImpl : public OptionBasedFactoryImpl, public SequencerFactory {
public:
  SequencerFactoryImpl(const Options& options);
  SequencerPtr create(Envoy::TimeSource& time_source, Envoy::Event::Dispatcher& dispatcher,
                      Envoy::MonotonicTime start_time,
                      BenchmarkClient& benchmark_client) const override;
};

class StoreFactoryImpl : public OptionBasedFactoryImpl, public StoreFactory {
public:
  StoreFactoryImpl(const Options& options);
  Envoy::Stats::StorePtr create() const override;
};

class StatisticFactoryImpl : public OptionBasedFactoryImpl, public StatisticFactory {
public:
  StatisticFactoryImpl(const Options& options);
  StatisticPtr create() const override;
};

class OutputFormatterFactoryImpl : public OutputFormatterFactory {
public:
  OutputFormatterPtr
  create(const nighthawk::client::OutputFormat_OutputFormatOptions output_format) const override;
};

class RequestSourceFactoryImpl : public OptionBasedFactoryImpl, public RequestSourceFactory {
public:
  RequestSourceFactoryImpl(const Options& options);
  RequestSourcePtr create(Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                          Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                          absl::string_view service_cluster_name) const override;

private:
  void setRequestHeader(Envoy::Http::HeaderMap& header, absl::string_view key,
                        absl::string_view value) const;
};

} // namespace Client
} // namespace Nighthawk
