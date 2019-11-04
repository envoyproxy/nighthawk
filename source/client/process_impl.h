#pragma once

#include <map>

#include "envoy/api/api.h"
#include "envoy/network/address.h"
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
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/http/context_impl.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/secret/secret_manager_impl.h"
#include "external/envoy/source/common/stats/allocator_impl.h"
#include "external/envoy/source/common/stats/thread_local_store.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"
#include "external/envoy/source/common/upstream/cluster_manager_impl.h"
#include "external/envoy/source/exe/platform_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/source/extensions/transport_sockets/tls/context_manager_impl.h"
#include "external/envoy/source/server/config_validation/admin.h"

#include "client/benchmark_client_impl.h"
#include "client/factories_impl.h"

namespace Nighthawk {
namespace Client {

/**
 * Only a single instance is allowed at a time machine-wide in this implementation.
 * Running multiple instances at the same might introduce noise into the measurements.
 * If there turns out to be a desire to run multiple instances at the same time, we could
 * introduce a --lock-name option. Note that multiple instances in the same process may
 * be problematic because of Envoy enforcing a single runtime instance.
 */
class ProcessImpl : public Process, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  ProcessImpl(const Options& options, Envoy::Event::TimeSystem& time_system);
  ~ProcessImpl() override;

  uint32_t determineConcurrency() const;
  bool run(OutputCollector& collector) override;
  void addTracingCluster(envoy::config::bootstrap::v2::Bootstrap& bootstrap, const Uri& uri) const;
  void setupTracingImplementation(envoy::config::bootstrap::v2::Bootstrap& bootstrap,
                                  const Uri& uri) const;
  void createBootstrapConfiguration(envoy::config::bootstrap::v2::Bootstrap& bootstrap,
                                    const Uri& uri, int number_of_workers) const;
  void maybeCreateTracingDriver(const envoy::config::trace::v2::Tracing& configuration);
  void shutdown() override;

private:
  void configureComponentLogLevels(spdlog::level::level_enum level);
  const std::vector<ClientWorkerPtr>& createWorkers(const uint32_t concurrency,
                                                    bool prefetch_connections);
  std::vector<StatisticPtr> vectorizeStatisticPtrMap(const StatisticFactory& statistic_factory,
                                                     const StatisticPtrMap& statistics) const;
  std::vector<StatisticPtr>
  mergeWorkerStatistics(const StatisticFactory& statistic_factory,
                        const std::vector<ClientWorkerPtr>& workers) const;
  Envoy::ProcessWide process_wide_;
  Envoy::PlatformImpl platform_impl_;
  Envoy::Event::TimeSystem& time_system_;
  StoreFactoryImpl store_factory_;
  Envoy::Stats::SymbolTableImpl symbol_table_;
  Envoy::Stats::AllocatorImpl stats_allocator_;
  Envoy::ThreadLocal::InstanceImpl tls_;
  Envoy::Stats::ThreadLocalStoreImpl store_root_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  std::vector<ClientWorkerPtr> workers_;
  const BenchmarkClientFactoryImpl benchmark_client_factory_;
  const TerminationPredicateFactoryImpl termination_predicate_factory_;
  const SequencerFactoryImpl sequencer_factory_;
  const HeaderSourceFactoryImpl header_generator_factory_;
  const Options& options_;

  Envoy::Init::ManagerImpl init_manager_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  Envoy::Server::ConfigTrackerImpl config_tracker_;
  Envoy::Secret::SecretManagerImpl secret_manager_;
  Envoy::Http::ContextImpl http_context_;
  Envoy::Thread::MutexBasicLockable access_log_lock_;
  Envoy::Singleton::ManagerPtr singleton_manager_;
  Envoy::AccessLog::AccessLogManagerImpl access_log_manager_;

  std::unique_ptr<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>
      ssl_context_manager_;

  std::unique_ptr<Envoy::Upstream::ProdClusterManagerFactory> cluster_manager_factory_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_{};
  std::unique_ptr<Runtime::ScopedLoaderSingleton> runtime_singleton_;
  Envoy::Init::WatcherImpl init_watcher_;
  Tracing::HttpTracerPtr http_tracer_;
  Envoy::Server::ValidationAdmin admin_;
  Envoy::ProtobufMessage::ProdValidationContextImpl validation_context_;
  bool shutdown_{true};
};

} // namespace Client
} // namespace Nighthawk
