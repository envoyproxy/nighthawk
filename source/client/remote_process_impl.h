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
  RemoteProcessImpl(const Options& options);
  bool run(OutputCollector& collector) override;
  void shutdown() override{};

private:
  const Options& options_;
};

} // namespace Client
} // namespace Nighthawk
