#pragma once

#include "envoy/network/address.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"
#include "nighthawk/client/process.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "common/api/api_impl.h"
#include "common/common/logger.h"
#include "common/common/thread_impl.h"
#include "common/event/real_time_system.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/thread_local/thread_local_impl.h"
#include "common/uri_impl.h"

#include "exe/process_wide.h"

#include "client/benchmark_client_impl.h"
#include "client/factories_impl.h"

#include "common/upstream/cluster_manager_impl.h"

#include "common/stats/allocator_impl.h"
#include "common/stats/fake_symbol_table_impl.h"
#include "common/stats/thread_local_store.h"
#include "server/server.h"

#include "envoy/tracing/http_tracer.h"

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
  ProcessImpl(const Options& options, Envoy::Event::TimeSystem& time_system,
              const PlatformUtil& platform_util);

  uint32_t determineConcurrency() const;
  bool run(OutputCollector& collector) override;

private:
  void configureComponentLogLevels(spdlog::level::level_enum level);
  const std::vector<ClientWorkerPtr>& createWorkers(const UriImpl& uri, const uint32_t concurrency);
  std::vector<StatisticPtr> vectorizeStatisticPtrMap(const StatisticFactory& statistic_factory,
                                                     const StatisticPtrMap& statistics) const;
  std::vector<StatisticPtr>
  mergeWorkerStatistics(const StatisticFactory& statistic_factory,
                        const std::vector<ClientWorkerPtr>& workers) const;

  std::map<std::string, uint64_t>
  mergeWorkerCounters(const std::vector<ClientWorkerPtr>& workers) const;

  Envoy::ProcessWide process_wide_;
  Envoy::Thread::ThreadFactoryImplPosix thread_factory_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  Envoy::Event::TimeSystem& time_system_;
  StoreFactoryImpl store_factory_;
  Envoy::Stats::FakeSymbolTableImpl symbol_table_;

  // Envoy::Stats::SymbolTableImpl symbol_table_;
  Envoy::Stats::AllocatorImpl stats_allocator_;
  Envoy::Stats::ThreadLocalStoreImpl store_root_;
  std::unique_ptr<Envoy::Server::ServerStats> server_stats_;

  // Envoy::Stats::StorePtr store_;
  Envoy::Api::Impl api_;
  Envoy::ThreadLocal::InstanceImpl tls_;
  Envoy::Event::DispatcherPtr dispatcher_;
  std::vector<ClientWorkerPtr> workers_;
  const Envoy::Cleanup cleanup_;
  const BenchmarkClientFactoryImpl benchmark_client_factory_;
  const SequencerFactoryImpl sequencer_factory_;
  const Options& options_;
  const PlatformUtil& platform_util_;

  Ssl::FakeAdmin admin_;
  Envoy::Init::ManagerImpl init_manager_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  Envoy::Server::ConfigTrackerImpl config_tracker_;
  Envoy::Secret::SecretManagerImpl secret_manager_;
  Envoy::Http::ContextImpl http_context_;
  Envoy::Thread::MutexBasicLockable fakelock_;
  Envoy::Singleton::ManagerPtr singleton_manager_;
  Envoy::AccessLog::AccessLogManagerImpl access_log_manager_;

  std::unique_ptr<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>
      ssl_context_manager_;

  std::unique_ptr<Envoy::Upstream::ProdClusterManagerFactory> cluster_manager_factory_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_{};
  std::unique_ptr<Runtime::ScopedLoaderSingleton> runtime_singleton_;
  Envoy::Init::WatcherImpl init_watcher_;
  Tracing::HttpTracerPtr http_tracer_;
};

} // namespace Client
} // namespace Nighthawk
