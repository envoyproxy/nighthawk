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
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/client_worker_impl.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"
#include "client/process_context_impl.h"

#include "api/client/output.pb.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

Main::Main(int argc, const char* const* argv)
    : Main(std::make_unique<Client::OptionsImpl>(argc, argv)) {}

Main::Main(Client::OptionsPtr&& options) : options_(std::move(options)) {}

bool Main::run() {
  Envoy::Thread::MutexBasicLockable log_lock;
  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(options_->verbosity()), "[%T.%f][%t][%L] %v", log_lock);
  ProcessContextImpl process_context(*options_);
  OutputFormatterFactoryImpl output_format_factory(process_context.time_system(), *options_);
  auto formatter = output_format_factory.create();
  if (process_context.run(*formatter)) {
    // TODO(oschaaf): the way we output should be optionized.
    std::cout << formatter->toString();
    ENVOY_LOG(info, "Done.");
    return true;
  }
  ENVOY_LOG(critical, "An error ocurred.");
  return false;
}

} // namespace Client
} // namespace Nighthawk
