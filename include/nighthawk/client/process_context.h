#pragma once

#include "envoy/network/address.h"
#include "envoy/stats/store.h"

#include "common/api/api_impl.h"
#include "common/common/logger.h"
#include "common/common/thread_impl.h"
#include "common/event/real_time_system.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/thread_local/thread_local_impl.h"

#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_formatter.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "common/uri_impl.h"

namespace Nighthawk {
namespace Client {

class ProcessContext {
public:
  virtual ~ProcessContext() = default;

  virtual void configureComponentLogLevels(spdlog::level::level_enum level) PURE;
  virtual uint32_t determineConcurrency() const PURE;
  virtual Envoy::Thread::ThreadFactory& thread_factory() PURE;
  virtual Envoy::Filesystem::Instance& file_system() PURE;
  virtual Envoy::Event::TimeSystem& time_system() PURE;
  virtual Envoy::Api::Impl& api() PURE;
  virtual Envoy::Event::Dispatcher& dispatcher() const PURE;
  virtual Envoy::ThreadLocal::Instance& tls() PURE;
  virtual Envoy::Stats::Store& store() const PURE;

  virtual const BenchmarkClientFactory& benchmark_client_factory() const PURE;
  virtual const SequencerFactory& sequencer_factory() const PURE;
  virtual const StoreFactory& store_factory() const PURE;

  virtual const std::vector<ClientWorkerPtr>& createWorkers(const UriImpl& uri,
                                                            const uint32_t concurrency) PURE;

  virtual std::vector<StatisticPtr>
  vectorizeStatisticPtrMap(const StatisticFactory& statistic_factory,
                           const StatisticPtrMap& statistics) const PURE;

  virtual bool run(OutputFormatter& formatter) PURE;
};

using ProcessContextPtr = std::unique_ptr<ProcessContext>;

} // namespace Client
} // namespace Nighthawk
