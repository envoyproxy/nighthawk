#pragma once

#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_formatter.h"
#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/source/common/utility.h"

namespace Nighthawk {
namespace Client {

class BenchmarkClientFactory {
public:
  virtual ~BenchmarkClientFactory() = default;
  virtual BenchmarkClientPtr create(Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher,
                                    Envoy::Stats::Store& store, const Uri uri) const PURE;
};

class SequencerFactory {
public:
  virtual ~SequencerFactory() = default;
  virtual SequencerPtr create(Envoy::TimeSource& time_source, Envoy::Event::Dispatcher& dispatcher,
                              Envoy::MonotonicTime start_time,
                              BenchmarkClient& benchmark_client) const PURE;
};

class StoreFactory {
public:
  virtual ~StoreFactory() = default;
  virtual Envoy::Stats::StorePtr create() const PURE;
};

class StatisticFactory {
public:
  virtual ~StatisticFactory() = default;
  virtual StatisticPtr create() const PURE;
};

class OutputFormatterFactory {
public:
  virtual ~OutputFormatterFactory() = default;
  virtual OutputFormatterPtr create() const PURE;
};

} // namespace Client
} // namespace Nighthawk
