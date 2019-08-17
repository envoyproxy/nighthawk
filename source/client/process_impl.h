#pragma once

#include <map>

#include "envoy/network/address.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"
#include "nighthawk/client/process.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "common/access_log/access_log_manager_impl.h"
#include "common/api/api_impl.h"
#include "common/common/logger.h"
#include "common/common/thread_impl.h"
#include "common/event/real_time_system.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/http/context_impl.h"
#include "common/secret/secret_manager_impl.h"
#include "common/stats/allocator_impl.h"
#include "common/stats/thread_local_store.h"
#include "common/thread_local/thread_local_impl.h"
#include "common/upstream/cluster_manager_impl.h"
#include "common/uri_impl.h"

#include "exe/process_wide.h"

#include "server/config_validation/admin.h"

#include "client/benchmark_client_impl.h"
#include "client/factories_impl.h"

#include "extensions/transport_sockets/tls/context_manager_impl.h"

#include "common/stats/allocator_impl.h"

namespace Nighthawk {

class NighthawkCounterImpl : public Envoy::Stats::Counter {
public:
  NighthawkCounterImpl(Envoy::Stats::CounterSharedPtr&& inner_counter)
      : inner_counter_(std::move(inner_counter)) {}

  std::string name() const override { return inner_counter_->name(); };
  Envoy::Stats::StatName statName() const override { return inner_counter_->statName(); };
  std::vector<Envoy::Stats::Tag> tags() const override { return inner_counter_->tags(); };
  std::string tagExtractedName() const override { return inner_counter_->tagExtractedName(); };
  Envoy::Stats::StatName tagExtractedStatName() const override {
    return inner_counter_->tagExtractedStatName();
  };
  void iterateTagStatNames(const Envoy::Stats::Counter::TagStatNameIterFn& fn) const override {
    inner_counter_->iterateTagStatNames(fn);
  };
  void iterateTags(const Envoy::Stats::Counter::TagIterFn& fn) const override {
    inner_counter_->iterateTags(fn);
  };
  bool used() const override { return inner_counter_->used(); };

  void add(uint64_t amount) override {
    if (!used()) {
      per_thread_counters_[std::this_thread::get_id()] = 0;
    }
    per_thread_counters_[std::this_thread::get_id()] += amount;
    inner_counter_->add(amount);
  };
  void inc() override { add(1); };
  uint64_t latch() override { return inner_counter_->latch(); };
  void reset() override { inner_counter_->reset(); };

  // We return the value we accumulated on this thread, if we have it.
  // If we don't have it, chances are we are being called from the main
  // thread. So we return the value from the inner counter, which will be
  // the merged/global value.
  uint64_t value() const override {
    auto it = per_thread_counters_.find(std::this_thread::get_id());
    if (it != per_thread_counters_.end()) {
      return it->second;
    } else {
      return inner_counter_->value();
    }
  };

  Envoy::Stats::SymbolTable& symbolTable() override { return inner_counter_->symbolTable(); }
  const Envoy::Stats::SymbolTable& constSymbolTable() const override {
    return inner_counter_->constSymbolTable();
  }

  // RefcountInterface
  void incRefCount() override { ++ref_count_; }
  bool decRefCount() override {
    ASSERT(ref_count_ >= 1);
    return --ref_count_ == 0;
  }
  uint32_t use_count() const override { return ref_count_; }

private:
  Envoy::Stats::CounterSharedPtr inner_counter_;
  std::atomic<uint16_t> ref_count_{0};
  std::map<std::thread::id, uint64_t> per_thread_counters_;
};

class NighthawkAllocatorImpl : public Envoy::Stats::AllocatorImpl {
public:
  using Envoy::Stats::AllocatorImpl::AllocatorImpl;

  Envoy::Stats::CounterSharedPtr makeCounter(Envoy::Stats::StatName name,
                                             absl::string_view tag_extracted_name,
                                             const std::vector<Envoy::Stats::Tag>& tags) override {
    auto counter = Envoy::Stats::AllocatorImpl::makeCounter(name, tag_extracted_name, tags);
    return Envoy::Stats::CounterSharedPtr(new NighthawkCounterImpl(std::move(counter)));
  }
};

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
  const envoy::config::bootstrap::v2::Bootstrap createBootstrapConfiguration(const Uri& uri) const;

private:
  void configureComponentLogLevels(spdlog::level::level_enum level);
  const std::vector<ClientWorkerPtr>& createWorkers(const UriImpl& uri, const uint32_t concurrency,
                                                    bool prefetch_connections);
  std::vector<StatisticPtr> vectorizeStatisticPtrMap(const StatisticFactory& statistic_factory,
                                                     const StatisticPtrMap& statistics) const;
  std::vector<StatisticPtr>
  mergeWorkerStatistics(const StatisticFactory& statistic_factory,
                        const std::vector<ClientWorkerPtr>& workers) const;

  std::map<std::string, uint64_t> countersToMap() const;

  Envoy::ProcessWide process_wide_;
  Envoy::Thread::ThreadFactoryImplPosix thread_factory_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  Envoy::Event::TimeSystem& time_system_;
  StoreFactoryImpl store_factory_;
  Envoy::Stats::SymbolTableImpl symbol_table_;
  NighthawkAllocatorImpl stats_allocator_;
  Envoy::Stats::ThreadLocalStoreImpl store_root_;
  Envoy::Api::Impl api_;
  Envoy::ThreadLocal::InstanceImpl tls_;
  Envoy::Event::DispatcherPtr dispatcher_;
  std::vector<ClientWorkerPtr> workers_;
  const Envoy::Cleanup cleanup_;
  const BenchmarkClientFactoryImpl benchmark_client_factory_;
  const SequencerFactoryImpl sequencer_factory_;
  const Options& options_;
  const PlatformUtil& platform_util_;

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
  Envoy::Server::ValidationAdmin admin_;
};

} // namespace Client
} // namespace Nighthawk
