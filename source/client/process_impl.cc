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

#include "external/envoy/source/common/api/api_impl.h"
#include "external/envoy/source/common/common/cleanup.h"
#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/init/manager_impl.h"
#include "external/envoy/source/common/local_info/local_info_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/singleton/manager_impl.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"
#include "external/envoy/source/extensions/tracers/well_known_names.h"

// TODO(oschaaf): See if we can leverage a static module registration like Envoy does to avoid the
// ifdefs in this file.
#ifdef ZIPKIN_ENABLED
#include "external/envoy/source/extensions/tracers/zipkin/zipkin_tracer_impl.h"
#endif
#include "external/envoy/source/extensions/transport_sockets/well_known_names.h"
#include "external/envoy/source/server/options_impl_platform.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "common/frequency.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"
#include "client/client.h"
#include "client/client_worker_impl.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"

#include "ares.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

// We customize ProdClusterManagerFactory for the sole purpose of returning our specialized
// http1 pool to the benchmark client, which allows us to offer connection prefetching.
class ClusterManagerFactory : public Envoy::Upstream::ProdClusterManagerFactory {
public:
  using Envoy::Upstream::ProdClusterManagerFactory::ProdClusterManagerFactory;

  Envoy::Http::ConnectionPool::InstancePtr allocateConnPool(
      Envoy::Event::Dispatcher& dispatcher, Envoy::Upstream::HostConstSharedPtr host,
      Envoy::Upstream::ResourcePriority priority, Envoy::Http::Protocol protocol,
      const Envoy::Network::ConnectionSocket::OptionsSharedPtr& options,
      const Envoy::Network::TransportSocketOptionsSharedPtr& transport_socket_options) override {
    if (protocol == Envoy::Http::Protocol::Http11 || protocol == Envoy::Http::Protocol::Http10) {
      return Envoy::Http::ConnectionPool::InstancePtr{
          new Http1PoolImpl(dispatcher, host, priority, options, transport_socket_options)};
    }
    return Envoy::Upstream::ProdClusterManagerFactory::allocateConnPool(
        dispatcher, host, priority, protocol, options, transport_socket_options);
  }
};

ProcessImpl::ProcessImpl(const Options& options, Envoy::Event::TimeSystem& time_system)
    : time_system_(time_system), store_factory_(options), stats_allocator_(symbol_table_),
      store_root_(stats_allocator_),
      api_(std::make_unique<Envoy::Api::Impl>(platform_impl_.threadFactory(), store_root_,
                                              time_system_, platform_impl_.fileSystem())),
      dispatcher_(api_->allocateDispatcher()), benchmark_client_factory_(options),
      sequencer_factory_(options), header_generator_factory_(options), options_(options),
      init_manager_("nh_init_manager"),
      local_info_(new Envoy::LocalInfo::LocalInfoImpl(
          {}, Envoy::Network::Utility::getLocalAddress(Envoy::Network::Address::IpVersion::v4),
          "nighthawk_service_zone", "nighthawk_service_cluster", "nighthawk_service_node")),
      secret_manager_(config_tracker_), http_context_(store_root_.symbolTable()),
      singleton_manager_(std::make_unique<Envoy::Singleton::ManagerImpl>(api_->threadFactory())),
      access_log_manager_(std::chrono::milliseconds(1000), *api_, *dispatcher_, access_log_lock_,
                          store_root_),
      init_watcher_("Nighthawk", []() {}), validation_context_(false, false) {
  std::string lower = absl::AsciiStrToLower(
      nighthawk::client::Verbosity::VerbosityOptions_Name(options_.verbosity()));
  configureComponentLogLevels(spdlog::level::from_str(lower));
}

ProcessImpl::~ProcessImpl() {
  RELEASE_ASSERT(shutdown_, "shutdown not called before destruction.");
}

void ProcessImpl::shutdown() {
  // Before we shut down the worker threads, stop threading.
  tls_.shutdownGlobalThreading();
  store_root_.shutdownThreading();
  // Before shutting down the cluster manager, stop the workers.
  for (auto& worker : workers_) {
    worker->shutdown();
  }
  workers_.clear();
  if (cluster_manager_ != nullptr) {
    cluster_manager_->shutdown();
  }
  tls_.shutdownThread();
  shutdown_ = true;
}

const std::vector<ClientWorkerPtr>& ProcessImpl::createWorkers(const uint32_t concurrency,
                                                               const bool prefetch_connections) {
  // TODO(oschaaf): Expose kMinimalDelay in configuration.
  const std::chrono::milliseconds kMinimalWorkerDelay = 500ms;
  ASSERT(workers_.empty());

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
        *api_, tls_, cluster_manager_, benchmark_client_factory_, sequencer_factory_,
        header_generator_factory_, store_root_, worker_number, first_worker_start + worker_delay,
        http_tracer_, prefetch_connections));
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
  uint32_t cpu_cores_with_affinity = Envoy::OptionsImplPlatform::getCpuCount();
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

void ProcessImpl::createBootstrapConfiguration(envoy::config::bootstrap::v2::Bootstrap& bootstrap,
                                               const Uri& uri, int number_of_clusters) const {
  for (int i = 0; i < number_of_clusters; i++) {
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

    cluster->set_name(fmt::format("{}", i));
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
  }
}

void ProcessImpl::addTracingCluster(envoy::config::bootstrap::v2::Bootstrap& bootstrap,
                                    const Uri& uri) const {
  auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
  cluster->set_name("tracing");
  cluster->mutable_connect_timeout()->set_seconds(options_.timeout().count());
  cluster->set_type(envoy::api::v2::Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);
  auto* host = cluster->add_hosts();
  auto* socket_address = host->mutable_socket_address();
  socket_address->set_address(uri.address()->ip()->addressAsString());
  socket_address->set_port_value(uri.port());
}

void ProcessImpl::setupTracingImplementation(envoy::config::bootstrap::v2::Bootstrap& bootstrap,
                                             const Uri& uri) const {
#ifdef ZIPKIN_ENABLED
  auto* http = bootstrap.mutable_tracing()->mutable_http();
  auto scheme = uri.scheme();
  const std::string kTracingClusterName = "tracing";
  http->set_name(fmt::format("envoy.{}", scheme));
  RELEASE_ASSERT(scheme == "zipkin", "Only zipkin is supported");
  envoy::config::trace::v2::ZipkinConfig config;
  config.mutable_collector_cluster()->assign(kTracingClusterName);
  config.mutable_collector_endpoint()->assign(std::string(uri.path()));
  config.mutable_shared_span_context()->set_value(true);
  http->mutable_typed_config()->PackFrom(config);
#else
  ENVOY_LOG(error, "Not build with any tracing support");
  UNREFERENCED_PARAMETER(bootstrap);
  UNREFERENCED_PARAMETER(uri);
#endif
}

void ProcessImpl::maybeCreateTracingDriver(const envoy::config::trace::v2::Tracing& configuration) {
  if (configuration.has_http()) {
#ifdef ZIPKIN_ENABLED
    std::string type = configuration.http().name();
    ENVOY_LOG(info, "loading tracing driver: {}", type);
    // Envoy::Server::Configuration::TracerFactory would be useful here to create the right
    // tracer implementation for us. However that ends up needing a Server::Instance to be passed
    // in which we do not have, and creating a fake for that means we risk code-churn because of
    // upstream code changes.
    auto& factory =
        Config::Utility::getAndCheckFactory<Envoy::Server::Configuration::TracerFactory>(
            configuration.http().name());
    ProtobufTypes::MessagePtr message = Envoy::Config::Utility::translateToFactoryConfig(
        configuration.http(), Envoy::ProtobufMessage::getStrictValidationVisitor(), factory);
    auto zipkin_config = dynamic_cast<const envoy::config::trace::v2::ZipkinConfig&>(*message);
    Envoy::Tracing::DriverPtr zipkin_driver =
        std::make_unique<Envoy::Extensions::Tracers::Zipkin::Driver>(
            zipkin_config, *cluster_manager_, store_root_, tls_,
            Envoy::Runtime::LoaderSingleton::get(), *local_info_, generator_, time_system_);
    http_tracer_ =
        std::make_unique<Envoy::Tracing::HttpTracerImpl>(std::move(zipkin_driver), *local_info_);
    http_context_.setTracer(*http_tracer_);
#else
    ENVOY_LOG(error, "Not build with any tracing support");
#endif
  }
}

void ProcessImpl::addHeaderSourceCluster(const Uri& uri,
                                         envoy::config::bootstrap::v2::Bootstrap& bootstrap) const {
  auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
  cluster->mutable_http2_protocol_options();
  cluster->set_name("headersource");
  cluster->set_type(envoy::api::v2::Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);
  cluster->mutable_connect_timeout()->set_seconds(options_.timeout().count());
  auto* host = cluster->add_hosts();
  auto* socket_address = host->mutable_socket_address();
  socket_address->set_address(uri.address()->ip()->addressAsString());
  socket_address->set_port_value(uri.port());
}

bool ProcessImpl::run(OutputCollector& collector) {
  UriImpl uri(options_.uri());
  UriPtr header_source_uri;
  UriPtr tracing_uri;

  try {
    // TODO(oschaaf): See if we can rid of resolving here.
    // We now only do it to validate.
    uri.resolve(*dispatcher_, Utility::translateFamilyOptionString(options_.addressFamily()));
    if (options_.headerSource() != "") {
      header_source_uri = std::make_unique<UriImpl>(options_.headerSource());
      header_source_uri->resolve(*dispatcher_,
                                 Utility::translateFamilyOptionString(options_.addressFamily()));
    }
    if (options_.trace() != "") {
      tracing_uri = std::make_unique<UriImpl>(options_.trace());
      tracing_uri->resolve(*dispatcher_,
                           Utility::translateFamilyOptionString(options_.addressFamily()));
    }
  } catch (UriException) {
    return false;
  }
  int number_of_workers = determineConcurrency();
  shutdown_ = false;
  const std::vector<ClientWorkerPtr>& workers =
      createWorkers(number_of_workers, options_.prefetchConnections());
  tls_.registerThread(*dispatcher_, true);
  store_root_.initializeThreading(*dispatcher_, tls_);
  runtime_singleton_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(
      Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(
          *dispatcher_, tls_, {}, *local_info_, init_manager_, store_root_, generator_,
          Envoy::ProtobufMessage::getStrictValidationVisitor(), *api_)});
  ssl_context_manager_ =
      std::make_unique<Extensions::TransportSockets::Tls::ContextManagerImpl>(time_system_);
  cluster_manager_factory_ = std::make_unique<ClusterManagerFactory>(
      admin_, Envoy::Runtime::LoaderSingleton::get(), store_root_, tls_, generator_,
      dispatcher_->createDnsResolver({}), *ssl_context_manager_, *dispatcher_, *local_info_,
      secret_manager_, validation_context_, *api_, http_context_, access_log_manager_,
      *singleton_manager_);
  envoy::config::bootstrap::v2::Bootstrap bootstrap;
  createBootstrapConfiguration(bootstrap, uri, number_of_workers);
  if (header_source_uri != nullptr) {
    addHeaderSourceCluster(*header_source_uri, bootstrap);
  }
  if (tracing_uri != nullptr) {
    setupTracingImplementation(bootstrap, *tracing_uri);
    addTracingCluster(bootstrap, *tracing_uri);
  }
  ENVOY_LOG(debug, "Computed configuration: {}", bootstrap.DebugString());
  cluster_manager_ = cluster_manager_factory_->clusterManagerFromProto(bootstrap);
  maybeCreateTracingDriver(bootstrap.tracing());
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
        collector.addResult(fmt::format("worker_{}", i),
                            vectorizeStatisticPtrMap(statistic_factory, worker->statistics()),
                            worker->thread_local_counter_values());
      }
      i++;
    }
  }
  if (ok) {
    StatisticFactoryImpl statistic_factory(options_);
    collector.addResult(
        "global", mergeWorkerStatistics(statistic_factory, workers),
        Utility().mapCountersFromStore(
            store_root_, [](absl::string_view, uint64_t value) { return value > 0; }));
  }

  return ok;
}

} // namespace Client
} // namespace Nighthawk
