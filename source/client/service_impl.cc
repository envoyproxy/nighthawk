#include "client/service_impl.h"
#include "client/service.grpc.pb.h"

#include <grpc++/grpc++.h>

#include "common/common/lock_guard.h"

#include "client/client.h"
#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

void ServiceImpl::NighthawkRunner(nighthawk::client::SendCommandRequest request) {
  OptionsPtr options = std::make_unique<OptionsImpl>(request.options());
  Envoy::Thread::MutexBasicLockable log_lock;
  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(options->verbosity()), "[%T.%f][%t][%L] %v", log_lock);
  ENVOY_LOG(info, "starting {}", request.options().DebugString());
  process_ = std::make_unique<ProcessImpl>(*options, time_system_);
  OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
  auto formatter = output_format_factory.create();
  nighthawk::client::SendCommandResponse response;

  if (process_->run(*formatter)) {
    *(response.mutable_output()->Add()) = formatter->toProto();
  }
  /*
  // TODO(oschaaf): communicate this actual failure to run.
  else {
    return grpc::Status(grpc::StatusCode::INTERNAL, "NH failed to execute");
  }*/
  response_queue_.Push(response);
  process_.reset();
}

bool ServiceImpl::EmitResponses(
    ::grpc::ServerReaderWriter<::nighthawk::client::SendCommandResponse,
                               ::nighthawk::client::SendCommandRequest>* stream) {
  while (!response_queue_.IsEmpty()) {
    auto response = response_queue_.Pop();
    if (!stream->Write(response)) {
      ENVOY_LOG(info, "Stream write failed");
      return false;
    }
  }
  return true;
}

// TODO(oschaaf): handle Process.run returning an error correctly.
// TODO(oschaaf): implement a way to cancel test runs, and update configuration on the fly.
// TODO(oschaaf): create MockProcess & use in service_test.cc
// TODO(oschaaf): add some logging to this.
// TODO(oschaaf): unit-test BlockingQueue
// TODO(oschaaf): unit-test Process
// TODO(oschaaf): unit-test the new OptionImpl constructor that takes a proto arg.
// TODO(oschaaf): move to async grpc server so we can process updates while running a benchmark
// TODO(oschaaf): validate options, sensible defaults. consider abusing TCLAP for both
// TODO(oschaaf): aggregate the logs and forward them in the grpc result-response.
::grpc::Status ServiceImpl::SendCommand(
    ::grpc::ServerContext* context,
    ::grpc::ServerReaderWriter<::nighthawk::client::SendCommandResponse,
                               ::nighthawk::client::SendCommandRequest>* stream) {

  bool error = false;
  try {
    nighthawk::client::SendCommandRequest request;
    while (!error && stream->Read(&request)) {
      switch (request.command_type()) {
      case nighthawk::client::SendCommandRequest_CommandType::SendCommandRequest_CommandType_kStart:
        if (nighthawk_runner_thread_.joinable()) {
          ENVOY_LOG(error, "Only a single benchmark session is allowed at a time.");
          error = true;
        } else {
          nighthawk_runner_thread_ = std::thread(&ServiceImpl::NighthawkRunner, this, request);
        }
        break;
      default:
        NOT_REACHED_GCOVR_EXCL_LINE;
      }
    }
    // TODO(oschaaf): which exceptions do we want to catch?
  } catch (std::exception& e) {
    ENVOY_LOG(error, "Exception: {}", e.what());
    error = true;
  }

  nighthawk_runner_thread_.join();
  EmitResponses(stream);
  ENVOY_LOG(info, "Server side done");
  return error ? grpc::Status(grpc::StatusCode::INTERNAL, "NH encountered an exception")
               : grpc::Status::OK;
}

} // namespace Client
} // namespace Nighthawk