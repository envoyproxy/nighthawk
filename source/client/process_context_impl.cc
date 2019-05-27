#include "client/client.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

#include "envoy/stats/store.h"

#include "nighthawk/client/output_formatter.h"

#include "common/api/api_impl.h"
#include "common/common/cleanup.h"
#include "common/common/thread_impl.h"
#include "common/event/dispatcher_impl.h"
#include "common/event/real_time_system.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/frequency.h"
#include "common/network/utility.h"
#include "common/runtime/runtime_impl.h"
#include "common/thread_local/thread_local_impl.h"
#include "common/utility.h"

#include "client/client_worker_impl.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"
#include "client/process_context_impl.h"
#include "common/uri_impl.h"

#include "api/client/output.pb.h"
#include "ares.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

ProcessContextImpl::ProcessContextImpl(const Options& options)
    : store_factory_(options), store_(store_factory_.create()),
      api_(thread_factory_, *store_, time_system_, file_system_),
      dispatcher_(api().allocateDispatcher()), cleanup_([this] { tls_.shutdownGlobalThreading(); }),
      benchmark_client_factory_(options), sequencer_factory_(options), options_(options)

{
  ares_library_init(ARES_LIB_INIT_ALL);
  Envoy::Event::Libevent::Global::initialize();
  configureComponentLogLevels(spdlog::level::from_str(options_.verbosity()));
  tls_.registerThread(*dispatcher_, true);
}

Envoy::Thread::ThreadFactory& ProcessContextImpl::thread_factory() { return thread_factory_; };
Envoy::Filesystem::Instance& ProcessContextImpl::file_system() { return file_system_; }
Envoy::Event::TimeSystem& ProcessContextImpl::time_system() { return time_system_; }
Envoy::Api::Impl& ProcessContextImpl::api() { return api_; }
Envoy::Event::Dispatcher& ProcessContextImpl::dispatcher() const { return *dispatcher_; }
Envoy::ThreadLocal::Instance& ProcessContextImpl::tls() { return tls_; }
Envoy::Stats::Store& ProcessContextImpl::store() const { return *store_; }

const BenchmarkClientFactory& ProcessContextImpl::benchmark_client_factory() const {
  return benchmark_client_factory_;
}
const SequencerFactory& ProcessContextImpl::sequencer_factory() const { return sequencer_factory_; }
const StoreFactory& ProcessContextImpl::store_factory() const { return store_factory_; };

const std::vector<ClientWorkerPtr>& ProcessContextImpl::createWorkers(const UriImpl& uri,
                                                                      const uint32_t concurrency) {
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
  const auto first_worker_start = time_system().monotonicTime() + kMinimalWorkerDelay;
  const double inter_worker_delay_usec =
      (1. / options_.requestsPerSecond()) * 1000000 / concurrency;
  int worker_number = 0;
  while (workers_.size() < concurrency) {
    const auto worker_delay = std::chrono::duration_cast<std::chrono::nanoseconds>(
        ((inter_worker_delay_usec * worker_number) * 1us));
    workers_.push_back(std::make_unique<ClientWorkerImpl>(
        api_, tls_, benchmark_client_factory_, sequencer_factory_, std::make_unique<UriImpl>(uri),
        store_factory_.create(), worker_number, first_worker_start + worker_delay));
    worker_number++;
  }
  return workers_;
}

ProcessContextImpl::~ProcessContextImpl() { ares_library_cleanup(); }

void ProcessContextImpl::configureComponentLogLevels(spdlog::level::level_enum level) {
  // TODO(oschaaf): Add options to tweak the log level of the various log tags
  // that are available.
  Envoy::Logger::Registry::setLogLevel(level);
  Envoy::Logger::Logger* logger_to_change = Envoy::Logger::Registry::logger("main");
  logger_to_change->setLevel(level);
}

uint32_t ProcessContextImpl::determineConcurrency() const {
  uint32_t cpu_cores_with_affinity = PlatformUtils::determineCpuCoresWithAffinity();
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
ProcessContextImpl::vectorizeStatisticPtrMap(const StatisticFactory& statistic_factory,
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
ProcessContextImpl::mergeWorkerStatistics(const StatisticFactory& statistic_factory,
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
ProcessContextImpl::mergeWorkerCounters(const std::vector<ClientWorkerPtr>& workers) const {
  std::map<std::string, uint64_t> merged;
  for (auto& w : workers) {
    const auto counters = Utility().mapCountersFromStore(
        w->store(), [](absl::string_view, uint64_t value) { return value > 0; });
    for (const auto& counter : counters) {
      if (merged.count(counter.first) == 0) {
        merged[counter.first] = counter.second;
      } else {
        merged[counter.first] += counter.second;
      }
    }
  }

  return merged;
}

bool ProcessContextImpl::run(OutputFormatter& formatter) {
  UriImpl uri(options_.uri());
  try {
    uri.resolve(*dispatcher_, Utility::parseAddressFamilyOptionString(options_.addressFamily()));
  } catch (UriException) {
    return false;
  }
  const std::vector<ClientWorkerPtr>& workers = createWorkers(uri, determineConcurrency());

  bool ok = true;
  Envoy::Runtime::RandomGeneratorImpl generator;
  Envoy::Runtime::ScopedLoaderSingleton loader(
      Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl({}, generator, store(), tls())});

  for (auto& w : workers_) {
    w->start();
  }

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
        formatter.addResult(
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
    formatter.addResult("global", mergeWorkerStatistics(statistic_factory, workers),
                        mergeWorkerCounters(workers));
  }
  return ok;
}

} // namespace Client
} // namespace Nighthawk
