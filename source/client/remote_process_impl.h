#pragma once

#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"
#include "nighthawk/client/process.h"

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {
namespace Client {

/**
 * Will delegate execution to a remote nighthawk_service using gRPC.
 */
class RemoteProcessImpl : public Process, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * @param options Options to send to the remote nighthawk service, as well as
   * containing information to connect to it (which won't be forwarded).
   */
  RemoteProcessImpl(const Options& options);
  /**
   * @param collector Collects the output from the remote nighthawk service.
   * @return true iff the remote execution should be considered successful.
   */
  bool run(OutputCollector& collector) override;
  /**
   * Shuts down the service, a no-op in this implementation.
   */
  void shutdown() override{};

private:
  const Options& options_;
};

} // namespace Client
} // namespace Nighthawk
