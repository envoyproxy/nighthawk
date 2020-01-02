#include "client/client.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

#include "envoy/stats/store.h"

#include "nighthawk/client/output_collector.h"

#include "external/envoy/source/common/common/cleanup.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"

#include "api/client/output.pb.h"

#include "common/frequency.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/client_worker_impl.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"
#include "client/output_collector_impl.h"
#include "client/process_impl.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

Main::Main(int argc, const char* const* argv)
    : Main(std::make_unique<Client::OptionsImpl>(argc, argv)) {}

Main::Main(Client::OptionsPtr&& options) : options_(std::move(options)) {}

bool Main::run() {
  Envoy::Thread::MutexBasicLockable log_lock;
  std::string lower = absl::AsciiStrToLower(
      nighthawk::client::Verbosity::VerbosityOptions_Name(options_->verbosity()));
  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(lower), "[%T.%f][%t][%L] %v", log_lock, false);
  Envoy::Event::RealTimeSystem time_system; // NO_CHECK_FORMAT(real_time)
  ProcessPtr process;
  if (options_->nighthawkService() != "") {
    process = std::make_unique<RemoteProcessImpl>(*options_);
  } else {
    process = std::make_unique<ProcessImpl>(*options_, time_system);
  }
  OutputFormatterFactoryImpl output_formatter_factory;
  OutputCollectorImpl output_collector(time_system, *options_);
  const bool res = process->run(output_collector);
  auto formatter = output_formatter_factory.create(options_->outputFormat());
  std::cout << formatter->formatProto(output_collector.toProto());
  process->shutdown();
  if (!res) {
    ENVOY_LOG(error, "An error ocurred.");
  } else {
    ENVOY_LOG(info, "Done.");
  }
  return res;
}

} // namespace Client
} // namespace Nighthawk