#include "client/process_impl.h"

#include <sys/file.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

#include "envoy/server/filter_config.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/output_collector.h"

#include "common/api/api_impl.h"
#include "common/common/cleanup.h"
#include "common/common/thread_impl.h"
#include "common/config/utility.h"
#include "common/event/dispatcher_impl.h"
#include "common/event/real_time_system.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/frequency.h"
#include "common/init/manager_impl.h"
#include "common/local_info/local_info_impl.h"
#include "common/network/utility.h"
#include "common/protobuf/message_validator_impl.h"
#include "common/runtime/runtime_impl.h"
#include "common/singleton/manager_impl.h"
#include "common/thread_local/thread_local_impl.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"
#include "client/client.h"
#include "client/client_worker_impl.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"

#include "extensions/tracers/well_known_names.h"
#include "extensions/tracers/zipkin/zipkin_tracer_impl.h"
#include "extensions/transport_sockets/well_known_names.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "ares.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

// We customize ProdClusterManagerFactory for the sole purpose of returning our specialized
// http1 pool to the benchmark client, which allows us to offer connection prefetching.
class ClusterManagerFactory : public Envoy::Upstream::ProdClusterManagerFactory {
public:
  using Envoy::Upstream::ProdClusterManagerFactory::ProdClusterManagerFactory;

  Envoy::Http::ConnectionPool::InstancePtr
  allocateConnPool(Envoy::Event::Dispatcher& dispatcher, Envoy::Upstream::HostConstSharedPtr host,
                   Envoy::Upstream::ResourcePriority priority, Envoy::Http::Protocol protocol,
                   const Envoy::Network::ConnectionSocket::OptionsSharedPtr& options) override {
    if (protocol == Envoy::Http::Protocol::Http11 || protocol == Envoy::Http::Protocol::Http10) {
      return Envoy::Http::ConnectionPool::InstancePtr{
          new Http1PoolImpl(dispatcher, host, priority, options)};
    }
    return Envoy::Upstream::ProdClusterManagerFactory::allocateConnPool(dispatcher, host, priority,
                                                                        protocol, options);
  }
};

ProcessImpl::ProcessImpl(const Options& options, Envoy::Event::TimeSystem& time_system,
                         const PlatformUtil& platform_util)
    : time_system_(time_system), store_factory_(options), stats_allocator_(symbol_table_),
      store_root_(stats_allocator_), api_(thread_factory_, store_root_, time_system_, file_system_),
      dispatcher_(api_.allocateDispatcher()), benchmark_client_factory_(options),
      sequencer_factory_(options), options_(options), platform_util_(platform_util),
      init_manager_("nh_init_manager"),
      local_info_(new Envoy::LocalInfo::LocalInfoImpl(
          {}, Envoy::Network::Utility::getLocalAddress(Envoy::Network::Address::IpVersion::v4),
          "nighthawk_service_zone", "nighthawk_service_cluster", "nighthawk_service_node")),
      secret_manager_(config_tracker_), http_context_(store_root_.symbolTable()),
      singleton_manager_(std::make_unique<Envoy::Singleton::ManagerImpl>(api_.threadFactory())),
      access_log_manager_(std::chrono::milliseconds(1000), api_, *dispatcher_, fakelock_,
                          store_root_),
      init_watcher_("Nighthawk", []() {}) {
  std::string lower = absl::AsciiStrToLower(
      nighthawk::client::Verbosity::VerbosityOptions_Name(options_.verbosity()));
  configureComponentLogLevels(spdlog::level::from_str(lower));
}

const std::vector<ClientWorkerPtr>& ProcessImpl::createWorkers(const UriImpl& uri,
                                                               const uint32_t concurrency,
                                                               const bool prefetch_connections) {
  // TODO(oschaaf): Expose kMinimalDelay in configuration.
  const std::chrono::milliseconds kMinimalWorkerDelay = 500ms;
  ASSERT(workers_.size() == 0);

  // We try to offset the start of each thread so that workers will execute tasks evenly spaced in
  // time. Let's assume we have two workers w0/w1, which should maintain a combined global pace of
  // 1000Hz. w0 and w1 both run at 500Hz, but ideally their execution is evenly spaced in time,
  // and not overlapping. Workers start offsets can be computed like
  // "worker_number*(1/global_frequency))", which would yield T0+[0ms, 1ms]. This helps reduce
  // batching/queueing effects, both initially, but also by calibrating the linear rate limiter we
  // currently have to a precise starting time, which helps later on.
  // TODO(oschaaf): Arguably, this ought to be the job of a rate limiter with awareness of the
  // global status quo, which we do not have right now. This has been noted in the
  // track-for-future issue.
  const auto first_worker_start = time_system_.monotonicTime() + kMinimalWorkerDelay;
  const double inter_worker_delay_usec =
      (1. / options_.requestsPerSecond()) * 1000000 / concurrency;
  int worker_number = 0;
  while (workers_.size() < concurrency) {
    const auto worker_delay = std::chrono::duration_cast<std::chrono::nanoseconds>(
        ((inter_worker_delay_usec * worker_number) * 1us));
    workers_.push_back(std::make_unique<ClientWorkerImpl>(
        api_, tls_, cluster_manager_, benchmark_client_factory_, sequencer_factory_,
        std::make_unique<UriImpl>(uri), store_root_, worker_number,
        first_worker_start + worker_delay, http_tracer_, prefetch_connections));
    worker_number++;
  }
  return workers_;
}

void ProcessImpl::configureComponentLogLevels(spdlog::level::level_enum level) {
  // TODO(oschaaf): Add options to tweak the log level of the various log tags
  // that are available.
  Envoy::Logger::Registry::setLogLevel(level);
  Envoy::Logger::Logger* logger_to_change = Envoy::Logger::Registry::logger("main");
  logger_to_change->setLevel(level);
}

uint32_t ProcessImpl::determineConcurrency() const {
  uint32_t cpu_cores_with_affinity = platform_util_.determineCpuCoresWithAffinity();
  if (cpu_cores_with_affinity == 0) {
    ENVOY_LOG(warn, "Failed to determine the number of cpus with affinity to our thread.");
    cpu_cores_with_affinity = std::thread::hardware_concurrency();
  }

  bool autoscale = options_.concurrency() == "auto";
  // TODO(oschaaf): Maybe, in the case where the concurrency flag is left out, but
  // affinity is set / we don't have affinity with all cores, we should default to autoscale.
  // (e.g. we are called via taskset).
  uint32_t concurrency = autoscale ? cpu_cores_with_affinity : std::stoi(options_.concurrency());

  if (autoscale) {
    ENVOY_LOG(info, "Detected {} (v)CPUs with affinity..", cpu_cores_with_affinity);
  }

  ENVOY_LOG(info, "Starting {} threads / event loops. Test duration: {} seconds.", concurrency,
            options_.duration().count());
  ENVOY_LOG(info, "Global targets: {} connections and {} calls per second.",
            options_.connections() * concurrency, options_.requestsPerSecond() * concurrency);

  if (concurrency > 1) {
    ENVOY_LOG(info, "   (Per-worker targets: {} connections and {} calls per second)",
              options_.connections(), options_.requestsPerSecond());
  }

  return concurrency;
}

std::vector<StatisticPtr>
ProcessImpl::vectorizeStatisticPtrMap(const StatisticFactory& statistic_factory,
                                      const StatisticPtrMap& statistics) const {
  std::vector<StatisticPtr> v;
  for (const auto& statistic : statistics) {
    auto new_statistic = statistic_factory.create()->combine(*(statistic.second));
    new_statistic->setId(statistic.first);
    v.push_back(std::move(new_statistic));
  }
  return v;
}

std::vector<StatisticPtr>
ProcessImpl::mergeWorkerStatistics(const StatisticFactory& statistic_factory,
                                   const std::vector<ClientWorkerPtr>& workers) const {
  // First we init merged_statistics with newly created statistics instances.
  // We do that by adding the same amount of Statistic instances that the first worker has.
  // (We always have at least one worker, and all workers have the same number of Statistic
  // instances associated to them, in the same order).
  std::vector<StatisticPtr> merged_statistics;
  StatisticPtrMap w0_statistics = workers[0]->statistics();
  for (const auto& w0_statistic : w0_statistics) {
    auto new_statistic = statistic_factory.create();
    new_statistic->setId(w0_statistic.first);
    merged_statistics.push_back(std::move(new_statistic));
  }

  // Merge the statistics of all workers into the statistics vector we initialized above.
  for (auto& w : workers) {
    uint32_t i = 0;
    for (const auto& wx_statistic : w->statistics()) {
      auto merged = merged_statistics[i]->combine(*(wx_statistic.second));
      merged->setId(merged_statistics[i]->id());
      merged_statistics[i] = std::move(merged);
      i++;
    }
  }
  return merged_statistics;
}

std::map<std::string, uint64_t>
ProcessImpl::mergeWorkerCounters(const std::vector<ClientWorkerPtr>& workers) const {
  std::map<std::string, uint64_t> merged;
  for (auto& w : workers) {
    const auto counters = Utility().mapCountersFromStore(
        w->store(), [](absl::string_view, uint64_t value) { return value > 0; });
    for (const auto& counter : counters) {
      if (merged.count(counter.first) == 0) {
        merged[counter.first] = counter.second;
      } else {
        // TODO(oschaaf): we used to sum the stats here, but when we switched to tls stats
        // the merging is done for us. We lost some information, which can be restored
        // in a follow-up.
        merged[counter.first] = counter.second;
      }
    }
  }

  return merged;
}

void ProcessImpl::createBootstrapConfiguration(envoy::config::bootstrap::v2::Bootstrap& bootstrap,
                                               const Uri& uri) const {
  auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
  if (uri.scheme() == "https") {
    auto* tls_context = cluster->mutable_tls_context();
    *tls_context = options_.tlsContext();
    auto* common_tls_context = tls_context->mutable_common_tls_context();
    if (options_.h2()) {
      common_tls_context->add_alpn_protocols("h2");
    } else {
      common_tls_context->add_alpn_protocols("http/1.1");
    }
  }

  cluster->set_name("client");
  cluster->mutable_connect_timeout()->set_seconds(options_.timeout().count());
  cluster->mutable_max_requests_per_connection()->set_value(options_.maxRequestsPerConnection());

  auto thresholds = cluster->mutable_circuit_breakers()->add_thresholds();
  // We do not support any retrying.
  thresholds->mutable_max_retries()->set_value(0);
  thresholds->mutable_max_connections()->set_value(options_.connections());
  thresholds->mutable_max_pending_requests()->set_value(options_.maxPendingRequests());
  thresholds->mutable_max_requests()->set_value(options_.maxActiveRequests());

  cluster->set_type(envoy::api::v2::Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);
  auto* host = cluster->add_hosts();
  auto* socket_address = host->mutable_socket_address();
  socket_address->set_address(uri.address()->ip()->addressAsString());
  socket_address->set_port_value(uri.port());

  ENVOY_LOG(info, "Computed configuration: {}", bootstrap.DebugString());
}

void ProcessImpl::addTracingCluster(envoy::config::bootstrap::v2::Bootstrap& bootstrap,
                                    const Uri& uri) const {
  auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
  if (uri.scheme() == "https") {
    RELEASE_ASSERT(false, "No tls config supported yet in tracing destination");
  }

  cluster->set_name("tracing");
  cluster->mutable_connect_timeout()->set_seconds(options_.timeout().count());
  cluster->set_type(envoy::api::v2::Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);
  auto* host = cluster->add_hosts();
  auto* socket_address = host->mutable_socket_address();
  socket_address->set_address(uri.address()->ip()->addressAsString());
  socket_address->set_port_value(uri.port());
  ENVOY_LOG(info, "Computed tracing configuration: {}", bootstrap.DebugString());
}

void ProcessImpl::setupTracingImplementation(
    envoy::config::bootstrap::v2::Bootstrap& bootstrap) const {
  auto* http = bootstrap.mutable_tracing()->mutable_http();
  http->set_name("envoy.zipkin");
  envoy::config::trace::v2::ZipkinConfig zipkin_config;
  zipkin_config.mutable_collector_cluster()->assign("tracing");
  zipkin_config.mutable_collector_endpoint()->assign("/api/v1/spans");
  zipkin_config.mutable_shared_span_context()->set_value(true);
  http->mutable_typed_config()->PackFrom(zipkin_config);
  ENVOY_LOG(info, "Computed tracing setup: {}", bootstrap.DebugString());
}

bool ProcessImpl::run(OutputCollector& collector) {
  UriImpl uri(options_.uri());
  UriImpl tracing_uri("http://127.0.0.1:9411/api/v1/spans");

  try {
    uri.resolve(*dispatcher_, Utility::translateFamilyOptionString(options_.addressFamily()));
    tracing_uri.resolve(*dispatcher_,
                        Utility::translateFamilyOptionString(options_.addressFamily()));
  } catch (UriException) {
    // XXX(oschaaf):
    tls_.shutdownGlobalThreading();
    return false;
  }
  const std::vector<ClientWorkerPtr>& workers =
      createWorkers(uri, determineConcurrency(), options_.prefetchConnections());

  tls_.registerThread(*dispatcher_, true);
  store_root_.initializeThreading(*dispatcher_, tls_);
  runtime_singleton_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(
      Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(
          *dispatcher_, tls_, {}, *local_info_, init_manager_, store_root_, generator_,
          Envoy::ProtobufMessage::getStrictValidationVisitor(), api_)});
  ssl_context_manager_ =
      std::make_unique<Extensions::TransportSockets::Tls::ContextManagerImpl>(time_system_);
  cluster_manager_factory_ = std::make_unique<ClusterManagerFactory>(
      admin_, Envoy::Runtime::LoaderSingleton::get(), store_root_, tls_, generator_,
      dispatcher_->createDnsResolver({}), *ssl_context_manager_, *dispatcher_, *local_info_,
      secret_manager_, Envoy::ProtobufMessage::getStrictValidationVisitor(), api_, http_context_,
      access_log_manager_, *singleton_manager_);

  envoy::config::bootstrap::v2::Bootstrap bootstrap;
  createBootstrapConfiguration(bootstrap, uri);
  addTracingCluster(bootstrap, tracing_uri);
  setupTracingImplementation(bootstrap);
  cluster_manager_ = cluster_manager_factory_->clusterManagerFromProto(bootstrap);

  auto& factory = Config::Utility::getAndCheckFactory<Envoy::Server::Configuration::TracerFactory>(
      bootstrap.tracing().http().name());
  ProtobufTypes::MessagePtr message = Envoy::Config::Utility::translateToFactoryConfig(
      bootstrap.tracing().http(), Envoy::ProtobufMessage::getStrictValidationVisitor(), factory);
  auto zipkin_config = dynamic_cast<const envoy::config::trace::v2::ZipkinConfig&>(*message);
  Envoy::Tracing::DriverPtr zipkin_driver =
      std::make_unique<Envoy::Extensions::Tracers::Zipkin::Driver>(
          zipkin_config, *cluster_manager_, store_root_, tls_,
          Envoy::Runtime::LoaderSingleton::get(), *local_info_, generator_, time_system_);
  http_tracer_ =
      std::make_unique<Envoy::Tracing::HttpTracerImpl>(std::move(zipkin_driver), *local_info_);
  http_context_.setTracer(*http_tracer_);

  cluster_manager_->setInitializedCb([this]() -> void { init_manager_.initialize(init_watcher_); });

  Runtime::LoaderSingleton::get().initialize(*cluster_manager_);

  for (auto& w : workers_) {
    w->start();
  }

  bool ok = true;
  for (auto& w : workers_) {
    w->waitForCompletion();
    ok = ok && w->success();
  }

  // We don't write per-worker results if we only have a single worker, because the global results
  // will be precisely the same.
  if (workers_.size() > 1) {
    int i = 0;
    for (auto& worker : workers_) {
      if (worker->success()) {
        StatisticFactoryImpl statistic_factory(options_);
        collector.addResult(
            fmt::format("worker_{}", i),
            vectorizeStatisticPtrMap(statistic_factory, worker->statistics()),
            Utility().mapCountersFromStore(
                worker->store(), [](absl::string_view, uint64_t value) { return value > 0; }));
      }
      i++;
    }
  }
  if (ok) {
    StatisticFactoryImpl statistic_factory(options_);
    collector.addResult("global", mergeWorkerStatistics(statistic_factory, workers),
                        mergeWorkerCounters(workers));
  }

  // Before we shut down the worker threads, stop threading.
  tls_.shutdownGlobalThreading();
  store_root_.shutdownThreading();
  // Before shutting down the cluster manager, stop the workers.
  workers_.clear();
  cluster_manager_->shutdown();
  return ok;
}

} // namespace Client
} // namespace Nighthawk
