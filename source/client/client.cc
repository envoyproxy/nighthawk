#include "source/client/client.h"

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

#include "source/client/client_worker_impl.h"
#include "source/client/factories_impl.h"
#include "source/client/options_impl.h"
#include "source/client/output_collector_impl.h"
#include "source/client/process_impl.h"
#include "source/client/remote_process_impl.h"
#include "source/common/frequency.h"
#include "source/common/signal_handler.h"
#include "source/common/uri_impl.h"
#include "source/common/utility.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

Main::Main(int argc, const char* const* argv)
    : Main(std::make_unique<Client::OptionsImpl>(argc, argv)) {}

Main::Main(Client::OptionsPtr&& options) : options_(std::move(options)) {}

bool Main::run() {
  Envoy::Thread::MutexBasicLockable log_lock;
  const std::string lower = absl::AsciiStrToLower(
      nighthawk::client::Verbosity::VerbosityOptions_Name(options_->verbosity()));
  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(lower), "[%T.%f][%t][%L] %v", log_lock, false);
  Envoy::Event::RealTimeSystem time_system; // NO_CHECK_FORMAT(real_time)
  ProcessPtr process;
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub;
  std::shared_ptr<grpc::Channel> channel;

  if (options_->nighthawkService() != "") {
    UriPtr uri;

    try {
      uri = std::make_unique<UriImpl>(options_->nighthawkService());
    } catch (const UriException&) {
      ENVOY_LOG(error, "Bad service uri: {}", options_->nighthawkService());
      return false;
    }

    channel = grpc::CreateChannel(fmt::format("{}:{}", uri->hostWithoutPort(), uri->port()),
                                  grpc::InsecureChannelCredentials());
    stub = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel);
    process = std::make_unique<RemoteProcessImpl>(*options_, *stub);
  } else {
    envoy::config::core::v3::TypedExtensionConfig typed_dns_resolver_config;
    Envoy::Network::DnsResolverFactory& dns_resolver_factory =
        Envoy::Network::createDefaultDnsResolverFactory(typed_dns_resolver_config);
    absl::StatusOr<ProcessPtr> process_or_status = ProcessImpl::CreateProcessImpl(
        *options_, dns_resolver_factory, std::move(typed_dns_resolver_config), time_system);
    if (!process_or_status.ok()) {
      ENVOY_LOG(error, "Unable to create ProcessImpl: {}", process_or_status.status().ToString());
      return false;
    }
    process = std::move(*process_or_status);
  }
  OutputFormatterFactoryImpl output_formatter_factory;
  OutputCollectorImpl output_collector(time_system, *options_);
  bool result;
  {
    auto signal_handler =
        std::make_unique<SignalHandler>([&process]() { process->requestExecutionCancellation(); });
    result = process->run(output_collector);
  }
  auto formatter = output_formatter_factory.create(options_->outputFormat());
  absl::StatusOr<std::string> formatted_proto = formatter->formatProto(output_collector.toProto());
  if (!formatted_proto.ok()) {
    ENVOY_LOG(error, "An error occurred while formatting proto");
    result = false;
  } else {
    std::cout << *formatted_proto;
  }
  process->shutdown();
  if (!result) {
    ENVOY_LOG(error, "An error ocurred.");
  } else {
    ENVOY_LOG(info, "Done.");
  }
  return result;
}

} // namespace Client
} // namespace Nighthawk
