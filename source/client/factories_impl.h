#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/factories.h"
#include "nighthawk/common/factories.h"
#include "nighthawk/common/termination_predicate.h"
#include "nighthawk/common/uri.h"

#include "common/platform_util_impl.h"

namespace Nighthawk {
namespace Client {

class OptionBasedFactoryImpl : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
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
                            Envoy::Tracing::HttpTracerSharedPtr& http_tracer,
                            absl::string_view cluster_name, int worker_id,
                            RequestSource& request_generator) const override;
};

class SequencerFactoryImpl : public OptionBasedFactoryImpl, public SequencerFactory {
public:
  SequencerFactoryImpl(const Options& options);
  SequencerPtr create(Envoy::TimeSource& time_source, Envoy::Event::Dispatcher& dispatcher,
                      const SequencerTarget& sequencer_target,
                      TerminationPredicatePtr&& termination_predicate, Envoy::Stats::Scope& scope,
                      const Envoy::MonotonicTime scheduled_starting_time) const override;
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
class RequestSourceConstructorImpl : public RequestSourceConstructorInterface {
public:
  virtual ~RequestSourceConstructorImpl() = default;
  RequestSourceConstructorImpl(const Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                               Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                               absl::string_view service_cluster_name);
  RequestSourcePtr createStaticRequestSource(Envoy::Http::RequestHeaderMapPtr&& base_header,
                                             const uint64_t max_yields = UINT64_MAX) const override;
  RequestSourcePtr createRemoteRequestSource(Envoy::Http::RequestHeaderMapPtr&& base_header,
                                             uint32_t header_buffer_length) const override;

protected:
  const Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::Scope& scope_;
  absl::string_view service_cluster_name_;
};
class RequestSourceFactoryImpl : public OptionBasedFactoryImpl, public RequestSourceFactory {
public:
  RequestSourceFactoryImpl(const Options& options);
  RequestSourcePtr
  create(const RequestSourceConstructorInterface& request_source_constructor) const override;

private:
  void setRequestHeader(Envoy::Http::RequestHeaderMap& header, absl::string_view key,
                        absl::string_view value) const;
};

class TerminationPredicateFactoryImpl : public OptionBasedFactoryImpl,
                                        public TerminationPredicateFactory {
public:
  TerminationPredicateFactoryImpl(const Options& options);
  TerminationPredicatePtr create(Envoy::TimeSource& time_source, Envoy::Stats::Scope& scope,
                                 const Envoy::MonotonicTime scheduled_starting_time) const override;
  TerminationPredicate* linkConfiguredPredicates(
      TerminationPredicate& last_predicate, const TerminationPredicateMap& predicates,
      const TerminationPredicate::Status termination_status, Envoy::Stats::Scope& scope) const;
};

} // namespace Client
} // namespace Nighthawk
