#pragma once

#include "envoy/network/address.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_formatter.h"
#include "nighthawk/client/process_context.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "common/api/api_impl.h"
#include "common/common/logger.h"
#include "common/common/thread_impl.h"
#include "common/event/real_time_system.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/thread_local/thread_local_impl.h"
#include "common/uri_impl.h"

#include "client/benchmark_client_impl.h"
#include "client/factories_impl.h"

namespace Nighthawk {
namespace Client {

constexpr const char* ProcessContextLockFile = "/tmp/nighthawk.lock";

/**
 * Only a single instance is allowed at a time machine-wide in this implementation.
 * Running multiple instances at the same might introduce noise into the measurements.
 * If there turns out to be a desire to run multiple instances at the same time, we could
 * introduce a --lock-name option. Note that multiple instances in the same process may
 * be problematic because of Envoy enforcing a single runtime instance.
 */
class ProcessContextImpl : public ProcessContext,
                           public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  ProcessContextImpl(const Options& options);
  ~ProcessContextImpl() override;

  uint32_t determineConcurrency() const override;

  Envoy::Event::TimeSystem& time_system() override;
  Envoy::Api::Impl& api() override;
  Envoy::Stats::Store& store() const override;

  bool run(OutputFormatter& formatter) override;

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

  Envoy::Thread::ThreadFactoryImplPosix thread_factory_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  StoreFactoryImpl store_factory_;
  Envoy::Stats::StorePtr store_;
  Envoy::Api::Impl api_;
  Envoy::ThreadLocal::InstanceImpl tls_;
  Envoy::Event::DispatcherPtr dispatcher_;
  std::vector<ClientWorkerPtr> workers_;
  const Envoy::Cleanup cleanup_;
  const BenchmarkClientFactoryImpl benchmark_client_factory_;
  const SequencerFactoryImpl sequencer_factory_;
  const Options& options_;
  int machine_lock_;
};

} // namespace Client
} // namespace Nighthawk
