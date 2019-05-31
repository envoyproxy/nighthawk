#pragma once

#include "envoy/network/address.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "common/api/api_impl.h"
#include "common/common/logger.h"
#include "common/event/real_time_system.h"

namespace Nighthawk {
namespace Client {

/**
 * Process context is shared between the CLI and grpc service. It is capable of executing
 * a full Nighthawk test run.
 */
class Process {
public:
  virtual ~Process() = default;

  /**
   * @param collector used to transform output into the desired format.
   * @return bool true iff execution was successfull.
   */
  virtual bool run(OutputCollector& collector) PURE;
};

using ProcessPtr = std::unique_ptr<Process>;

} // namespace Client
} // namespace Nighthawk
