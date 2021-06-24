#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/factories.h"
#include "nighthawk/common/factories.h"
#include "nighthawk/common/termination_predicate.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/config/utility.h"

#include "source/common/platform_util_impl.h"

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

class RequestSourceFactoryImpl : public OptionBasedFactoryImpl, public RequestSourceFactory {
public:
  RequestSourceFactoryImpl(const Options& options, Envoy::Api::Api& api);
  RequestSourcePtr create(const Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                          Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                          absl::string_view service_cluster_name) const override;

private:
  Envoy::Api::Api& api_;
  void setRequestHeader(Envoy::Http::RequestHeaderMap& header, absl::string_view key,
                        absl::string_view value) const;
  /**
   * Instantiates a RequestSource using a RequestSourcePluginFactory based on the plugin name in
   * |config|, unpacking the plugin-specific config proto within |config|. Validates the config
   * proto.
   *
   * @param config Proto containing plugin name and plugin-specific config proto.
   * @param api Api parameter that contains timesystem, filesystem, and threadfactory.
   * @param header Any headers in request specifiers yielded by the request
   * source plugin will override what is specified here.

   * @return absl::StatusOr<RequestSourcePtr> Initialized plugin or error status due to missing
   * plugin or config proto validation error.
   */
  absl::StatusOr<RequestSourcePtr>
  LoadRequestSourcePlugin(const envoy::config::core::v3::TypedExtensionConfig& config,
                          Envoy::Api::Api& api, Envoy::Http::RequestHeaderMapPtr header) const;
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
