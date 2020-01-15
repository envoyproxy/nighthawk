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
  ProcessImpl(const Options& options, Envoy::Event::TimeSystem& time_system);
  ~ProcessImpl() override;

  /**
   * @return uint32_t the concurrency we determined should run at based on configuration and
   * available machine resources.
   */
  uint32_t determineConcurrency() const;

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

  /**
   * Computes the SNI host based on the passed in uri(s), request headers, and protocol.
   * Given a vector of Uris containing a single entry, its host fragment will be considered.
   * Subsequently the passed request headers will be scanned for any host headers to override any
   * Uri-derived sni host. If the passed in protocol is HTTP/2 or higher, next to host, :authority
   * will be considered as well.
   *
   * @param parsed_uris The parsed target uris configured for the load test.
   * @param request_headers Request headers to scan.
   * @param protocol The anticipated protocol that will be used.
   * @return std::string The sni-host derived from the configured load test target Uris and any
   * host/authority request-headers found. Empty if no (unambiguous) sni host could be derived.
   */
  static std::string computeSniHost(const std::vector<UriPtr>& parsed_uris,
                                    const std::vector<std::string>& request_headers,
                                    const Envoy::Http::Protocol protocol);

private:
  /**
   * @brief Creates a cluster for usage by a remote request source.
   *
   * @param uri The parsed uri of the remote request source.
   * @param worker_number The worker number that we are creating this cluster for.
   * @param config The bootstrap configuration that will be modified.
   */
  void addRequestSourceCluster(const Uri& uri, int worker_number,
                               envoy::config::bootstrap::v3alpha::Bootstrap& config) const;
  void addTracingCluster(envoy::config::bootstrap::v3alpha::Bootstrap& bootstrap,
                         const Uri& uri) const;
  void setupTracingImplementation(envoy::config::bootstrap::v3alpha::Bootstrap& bootstrap,
                                  const Uri& uri) const;
  void createBootstrapConfiguration(envoy::config::bootstrap::v3alpha::Bootstrap& bootstrap,
                                    const std::vector<UriPtr>& uris,
                                    const UriPtr& request_source_uri, int number_of_workers) const;
  void maybeCreateTracingDriver(const envoy::config::trace::v3alpha::Tracing& configuration);

  void configureComponentLogLevels(spdlog::level::level_enum level);
  const std::vector<ClientWorkerPtr>& createWorkers(const uint32_t concurrency);
  std::vector<StatisticPtr> vectorizeStatisticPtrMap(const StatisticFactory& statistic_factory,
                                                     const StatisticPtrMap& statistics) const;
  std::vector<StatisticPtr>
  mergeWorkerStatistics(const StatisticFactory& statistic_factory,
                        const std::vector<ClientWorkerPtr>& workers) const;
  void setupForHRTimers();
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
  const RequestSourceFactoryImpl request_generator_factory_;
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

  std::unique_ptr<ClusterManagerFactory> cluster_manager_factory_;
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
