#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "api/client/options.pb.validate.h"
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
  bool success = process_->run(*formatter);
  nighthawk::client::SendCommandResponse response;
  *(response.mutable_output()->Add()) = formatter->toProto();
  response_queue_.Push(ServiceProcessResult(response, success));
  process_.reset();
}

bool ServiceImpl::EmitResponses(
    ::grpc::ServerReaderWriter<::nighthawk::client::SendCommandResponse,
                               ::nighthawk::client::SendCommandRequest>* stream) {
  while (!response_queue_.IsEmpty()) {
    auto result = response_queue_.Pop();
    // TODO(oschaaf): handle result.status == false;
    if (!stream->Write(result.response())) {
      ENVOY_LOG(info, "Stream write failed");
      return false;
    }
  }
  return true;
}

// TODO(oschaaf): implement a way to cancel test runs, and update configuration on the fly.
// TODO(oschaaf): create MockProcess & use in service_test.cc
// TODO(oschaaf): add some logging to this.
// TODO(oschaaf): unit-test BlockingQueue
// TODO(oschaaf): unit-test Process
// TODO(oschaaf): validate options, sensible defaults. consider abusing TCLAP for both
// TODO(oschaaf): aggregate the logs and forward them in the grpc result-response.
::grpc::Status ServiceImpl::SendCommand(
    ::grpc::ServerContext* /*context*/,
    ::grpc::ServerReaderWriter<::nighthawk::client::SendCommandResponse,
                               ::nighthawk::client::SendCommandRequest>* stream) {

  bool error = false;
  try {
    nighthawk::client::SendCommandRequest request;
    while (!error && stream->Read(&request)) {
      try {
        Envoy::MessageUtil::validate(request.options());
      } catch (Envoy::EnvoyException exception) {
        ENVOY_LOG(error, "Request options validation error: {}", exception.what());
        error = true;
        break;
      }
      if (!error) {
        switch (request.command_type()) {
        case nighthawk::client::SendCommandRequest_CommandType::
            SendCommandRequest_CommandType_kStart:
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