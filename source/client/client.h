#pragma once

#include "envoy/network/address.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"
#include "nighthawk/common/statistic.h"

#include "common/common/logger.h"

#include "process_impl.h"

namespace Nighthawk {
namespace Client {

class Main : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  Main(int argc, const char* const* argv);
  Main(Client::OptionsPtr&& options);
  bool run();

private:
  OptionsPtr options_;
  std::unique_ptr<Envoy::Logger::Context> logging_context_;
};

} // namespace Client
} // namespace Nighthawk
