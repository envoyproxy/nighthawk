#include "client/client.h"

#include <grpc++/grpc++.h>

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
#include "api/client/service.grpc.pb.h"

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
  bool res = false;
  OutputFormatterFactoryImpl output_formatter_factory;
  auto formatter = output_formatter_factory.create(options_->outputFormat());
  Envoy::Thread::MutexBasicLockable log_lock;

  // TODO(oschaaf): separate bugfix PR for this, we need to lowercase.
  std::string lower = absl::AsciiStrToLower(
      nighthawk::client::Verbosity::VerbosityOptions_Name(options_->verbosity()));
  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(lower), "[%T.%f][%t][%L] %v", log_lock, false);
  // Single shot remote execution PoC.
  // TODO(oschaaf): optionize, e.g: --remote-nighthawk-service 127.0.0.1:8443
  if (true) {
    auto channel = grpc::CreateChannel(fmt::format("{}:{}", "127.0.0.1", 8443),
                                       grpc::InsecureChannelCredentials());
    auto stub = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel);
    grpc::ClientContext context;
    nighthawk::client::ExecutionRequest request;
    nighthawk::client::ExecutionResponse response;

    *request.mutable_start_request()->mutable_options() = *options_->toCommandLineOptions();
    auto r = stub->ExecutionStream(&context);

    if (r->Write(request, {}) && r->Read(&response)) {
      if (response.has_output()) {
        std::cout << formatter->formatProto(response.output());
      } else {
        ENVOY_LOG(error, "remote execution failed");
      }
      if (response.has_error_detail()) {
        ENVOY_LOG(error, "have error detail: {}", response.error_detail().DebugString());
      }
      if (!r->WritesDone()) {
        ENVOY_LOG(warn, "writeDone() failed");
      } else {
        auto status = r->Finish();
        res = status.ok();
      }
    }
  } else {
    Envoy::Event::RealTimeSystem time_system; // NO_CHECK_FORMAT(real_time)
    ProcessImpl process(*options_, time_system);
    OutputCollectorImpl output_collector(time_system, *options_);
    res = process.run(output_collector);
    std::cout << formatter->formatProto(output_collector.toProto());
    process.shutdown();
  }
  if (!res) {
    ENVOY_LOG(error, "An error ocurred.");
  } else {
    ENVOY_LOG(info, "Done.");
  }
  return res;
}

} // namespace Client
} // namespace Nighthawk
