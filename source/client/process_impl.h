#pragma once

#include <map>

#include "envoy/api/api.h"
#include "envoy/network/address.h"
#include "envoy/server/instance.h"
#include "envoy/stats/store.h"
#include "envoy/tracing/http_tracer.h"

#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"
#include "nighthawk/client/process.h"
#include "nighthawk/common/statistic.h"

#include "external/envoy/source/common/access_log/access_log_manager_impl.h"
#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/random_generator.h"
#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/grpc/context_impl.h"
#include "external/envoy/source/common/http/context_impl.h"
#include "external/envoy/source/common/network/dns_resolver/dns_factory_util.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/quic/quic_stat_names.h"
#include "external/envoy/source/common/router/context_impl.h"
#include "external/envoy/source/common/secret/secret_manager_impl.h"
#include "external/envoy/source/common/stats/allocator_impl.h"
#include "external/envoy/source/common/stats/thread_local_store.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"
#include "external/envoy/source/common/upstream/cluster_manager_impl.h"
#include "external/envoy/source/exe/platform_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/source/extensions/transport_sockets/tls/context_manager_impl.h"
#include "external/envoy/source/server/config_validation/admin.h"
#include "external/envoy/source/server/options_impl.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"

#include "source/client/benchmark_client_impl.h"
#include "source/client/factories_impl.h"
#include "source/client/flush_worker_impl.h"

namespace Nighthawk {
namespace Client {

class ClusterManagerFactory;
/**
 * Only a single instance is allowed at a time machine-wide in this implementation.
 * Running multiple instances at the same might introduce noise into the measurements.
 * If there turns out to be a desire to run multiple instances at the same time, we could
 * introduce a --lock-name option. Note that multiple instances in the same process may
 * be problematic because of Envoy enforcing a single runtime instance.
 */
class ProcessImpl : public Process, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * Creates a ProcessImpl.
   * @param options provides the options configuration to be used.
   * @param dns_resolver_factory provides a pluggable factory to create a DNS resolver. The
   * resolver is used for resolving DNS names in the bootstrap and then by the cluster manager.
   * @param typed_dns_resolver_config the config that defined the dns_resolver_factory, also
   * needed as an input to createDnsResolver() on the factory.
   * @param time_system provides the Envoy::Event::TimeSystem implementation that will be used.
   * @param process_wide optional parameter which can be used to pass a pre-setup reference to
   * an active Envoy::ProcessWide instance. ProcessImpl will add a reference to this when passed,
   * and hold on that that throughout its lifetime.
   * If this parameter is not supplied, ProcessImpl will contruct its own Envoy::ProcessWide
   * instance.
   */
  static absl::StatusOr<ProcessPtr>
  CreateProcessImpl(const Options& options,
                    Envoy::Network::DnsResolverFactory& dns_resolver_factory,
                    envoy::config::core::v3::TypedExtensionConfig typed_dns_resolver_config,
                    Envoy::Event::TimeSystem& time_system,
                    const std::shared_ptr<Envoy::ProcessWide>& process_wide = nullptr);

  ~ProcessImpl() override;

  /**
   * Runs the process.
   *
   * @param collector output collector implementation which will collect and hold the native output
   * format.
   * @return true iff execution should be considered successful.
   */
  bool run(OutputCollector& collector) override;

  /**
   * Should be called before destruction to cleanly shut down.
   */
  void shutdown() override;

  bool requestExecutionCancellation() override;

private:
  // Use CreateProcessImpl to construct an instance of ProcessImpl.
  ProcessImpl(const Options& options, Envoy::Event::TimeSystem& time_system,
              Envoy::Network::DnsResolverFactory& dns_resolver_factory,
              envoy::config::core::v3::TypedExtensionConfig typed_dns_resolver_config,
              const std::shared_ptr<Envoy::ProcessWide>& process_wide = nullptr);

  void addTracingCluster(envoy::config::bootstrap::v3::Bootstrap& bootstrap, const Uri& uri) const;
  void setupTracingImplementation(envoy::config::bootstrap::v3::Bootstrap& bootstrap,
                                  const Uri& uri) const;
  void maybeCreateTracingDriver(const envoy::config::trace::v3::Tracing& configuration);
  void configureComponentLogLevels(spdlog::level::level_enum level);
  /**
   * Prepare the ProcessImpl instance by creating and configuring the workers it needs for execution
   * of the load test.
   *
   * @param concurrency the amount of workers that should be created.
   */
  void createWorkers(const uint32_t concurrency, const absl::optional<Envoy::SystemTime>& schedule);
  std::vector<StatisticPtr> vectorizeStatisticPtrMap(const StatisticPtrMap& statistics) const;
  std::vector<StatisticPtr>
  mergeWorkerStatistics(const std::vector<ClientWorkerPtr>& workers) const;
  void setupForHRTimers();
  /**
   * If there are sinks configured in bootstrap, populate stats_sinks with sinks
   * created through NighthawkStatsSinkFactory and add them to store_root_.
   *
   * @param bootstrap the bootstrap configuration which include the stats sink configuration.
   * @param stats_sinks a Sink list to be populated.
   */
  void setupStatsSinks(const envoy::config::bootstrap::v3::Bootstrap& bootstrap,
                       std::list<std::unique_ptr<Envoy::Stats::Sink>>& stats_sinks);
  bool runInternal(OutputCollector& collector, const UriPtr& tracing_uri,
                   const Envoy::Network::DnsResolverSharedPtr& dns_resolver,
                   const absl::optional<Envoy::SystemTime>& schedule);

  /**
   * Compute the offset at which execution should start. We adhere to the scheduled start passed in
   * as an argument when specified, otherwise we need a delay that will be sufficient for all the
   * workers to get up and running.
   *
   * @param time_system Time system used to obtain the current time.
   * @param scheduled_start Optional scheduled start.
   * @param concurrency The number of workers that will be used during execution.
   * @return Envoy::MonotonicTime Time at which execution should start.
   */
  static Envoy::MonotonicTime
  computeFirstWorkerStart(Envoy::Event::TimeSystem& time_system,
                          const absl::optional<Envoy::SystemTime>& scheduled_start,
                          const uint32_t concurrency);

  /**
   * We offset the start of each thread so that workers will execute tasks evenly spaced in
   * time. Let's assume we have two workers w0/w1, which should maintain a combined global pace of
   * 1000Hz. w0 and w1 both run at 500Hz, but ideally their execution is evenly spaced in time,
   * and not overlapping. Workers start offsets can be computed like
   * "worker_number*(1/global_frequency))", which would yield T0+[0ms, 1ms]. This helps reduce
   * batching/queueing effects, both initially, but also by calibrating the linear rate limiter we
   * currently have to a precise starting time, which helps later on.
   *
   * @param concurrency The number of workers that will be used during execution.
   * @param rps Anticipated requests per second during execution.
   * @return std::chrono::nanoseconds The delay that should be used as an offset between each
   * independent worker execution start.
   */
  static std::chrono::nanoseconds computeInterWorkerDelay(const uint32_t concurrency,
                                                          const uint32_t rps);

  const envoy::config::core::v3::Node node_;
  const Envoy::Protobuf::RepeatedPtrField<std::string> node_context_params_;
  const Options& options_;
  const int number_of_workers_;
  std::shared_ptr<Envoy::ProcessWide> process_wide_;
  Envoy::PlatformImpl platform_impl_;
  Envoy::Event::TimeSystem& time_system_;
  Envoy::Stats::SymbolTableImpl symbol_table_;
  Envoy::Stats::AllocatorImpl stats_allocator_;
  Envoy::ThreadLocal::InstanceImpl tls_;
  Envoy::Stats::ThreadLocalStoreImpl store_root_;
  Envoy::Quic::QuicStatNames quic_stat_names_;
  envoy::config::bootstrap::v3::Bootstrap bootstrap_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  std::vector<ClientWorkerPtr> workers_;
  const BenchmarkClientFactoryImpl benchmark_client_factory_;
  const TerminationPredicateFactoryImpl termination_predicate_factory_;
  const SequencerFactoryImpl sequencer_factory_;
  const RequestSourceFactoryImpl request_generator_factory_;
  Envoy::Init::ManagerImpl init_manager_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
  Envoy::Random::RandomGeneratorImpl generator_;
  Envoy::Server::ConfigTrackerImpl config_tracker_;
  Envoy::Secret::SecretManagerImpl secret_manager_;
  Envoy::Http::ContextImpl http_context_;
  Envoy::Grpc::ContextImpl grpc_context_;
  Envoy::Thread::MutexBasicLockable access_log_lock_;
  Envoy::Singleton::ManagerPtr singleton_manager_;
  Envoy::AccessLog::AccessLogManagerImpl access_log_manager_;
  Envoy::Network::DnsResolverFactory& dns_resolver_factory_;
  // Config that was used to create dns_resolver_factory_. Also must be provided when calling the
  // factory to create a resolver.
  envoy::config::core::v3::TypedExtensionConfig typed_dns_resolver_config_;

  std::unique_ptr<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>
      ssl_context_manager_;

  std::unique_ptr<ClusterManagerFactory> cluster_manager_factory_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_{};
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> runtime_singleton_;
  Envoy::Init::WatcherImpl init_watcher_;
  Envoy::Tracing::HttpTracerSharedPtr http_tracer_;
  Envoy::Server::ValidationAdmin admin_;
  Envoy::ProtobufMessage::ProdValidationContextImpl validation_context_;
  bool shutdown_{true};
  Envoy::Thread::MutexBasicLockable workers_lock_;
  bool cancelled_{false};
  std::unique_ptr<FlushWorkerImpl> flush_worker_;
  Envoy::Router::ContextImpl router_context_;
  Envoy::OptionsImpl envoy_options_;
  // Null server implementation used as a placeholder. Its methods should never get called
  // because Nighthawk is not a full Envoy server that performs xDS config validation.
  std::unique_ptr<Envoy::Server::Instance> server_;
  // Null server factory context implementation for the same reason as above.
  std::unique_ptr<Envoy::Server::Configuration::ServerFactoryContext> server_factory_context_;
};

} // namespace Client
} // namespace Nighthawk
