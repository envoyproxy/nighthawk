#include "client/client.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

#include "envoy/stats/store.h"

#include "external/envoy/source/common/api/api_impl.h"
#include "external/envoy/source/common/common/cleanup.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"

#include "nighthawk/client/output_collector.h"

#include "api/client/output.pb.h"

#include "common/common/thread_impl.h"
#include "common/filesystem/filesystem_impl.h" // XXX(oschaaf):
#include "common/frequency.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/client_worker_impl.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"
#include "client/process_impl.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

Main::Main(int argc, const char* const* argv)
    : Main(std::make_unique<Client::OptionsImpl>(argc, argv)) {}

Main::Main(Client::OptionsPtr&& options) : options_(std::move(options)) {}

bool Main::run() {
  Envoy::Thread::MutexBasicLockable log_lock;

  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(
          nighthawk::client::Verbosity::VerbosityOptions_Name(options_->verbosity())),
      "[%T.%f][%t][%L] %v", log_lock);
  Envoy::Event::RealTimeSystem time_system; // NO_CHECK_FORMAT(real_time)
  PlatformUtilImpl platform_util;
  ProcessImpl process(*options_, time_system, platform_util);
  OutputCollectorFactoryImpl output_format_factory(time_system, *options_);
  auto collector = output_format_factory.create();
  if (process.run(*collector)) {
    // TODO(oschaaf): the way we output should be optionized.
    std::cout << collector->toString();
    ENVOY_LOG(info, "Done.");
    return true;
  }
  ENVOY_LOG(critical, "An error ocurred.");
  return false;
}

} // namespace Client
} // namespace Nighthawk
