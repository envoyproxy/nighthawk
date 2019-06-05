#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "client/client.h"
#include "client/options_impl.h"

#include "api/client/options.pb.validate.h"

namespace Nighthawk {
namespace Client {

void ServiceImpl::NighthawkRunner(nighthawk::client::SendCommandRequest request) {
  Envoy::Thread::LockGuard lock(mutex_);
  OptionsPtr options = std::make_unique<OptionsImpl>(request.options());
  Envoy::Thread::MutexBasicLockable log_lock;
  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(options->verbosity()), "[%T.%f][%t][%L] %v", log_lock);
  process_ = std::make_unique<ProcessImpl>(*options, time_system_);

  // We perform this validation here because we need to rutime to be initialized
  // for this, something that ProcessContext does for us.
  try {
    Envoy::MessageUtil::validate(request.options());
  } catch (Envoy::EnvoyException exception) {
    nighthawk::client::SendCommandResponse response;
    response_queue_.Push(ServiceProcessResult(response, exception.what()));
    process_.reset();
    return;
  }

  OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
  auto formatter = output_format_factory.create();
  bool success = process_->run(*formatter);
  nighthawk::client::SendCommandResponse response;
  *(response.mutable_output()->Add()) = formatter->toProto();
  response_queue_.Push(ServiceProcessResult(response, success ? "" : "Unkown failure"));
  process_.reset();
}

void ServiceImpl::EmitResponses(
    ::grpc::ServerReaderWriter<::nighthawk::client::SendCommandResponse,
                               ::nighthawk::client::SendCommandRequest>* stream,
    std::string& error_messages) {

  while (!response_queue_.IsEmpty()) {
    auto result = response_queue_.Pop();
    if (!result.success()) {
      error_messages.append(result.error_message());
      continue;
    }
    // We just log write failures and proceed as usual; not much we can do.
    if (!stream->Write(result.response())) {
      ENVOY_LOG(warn, "Stream write failed");
    }
  }
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
  std::string error_message = "";

  nighthawk::client::SendCommandRequest request;
  while (stream->Read(&request)) {
    Envoy::Thread::LockGuard lock(mutex_);
    switch (request.command_type()) {
    case nighthawk::client::SendCommandRequest_CommandType::SendCommandRequest_CommandType_kStart:
      if (nighthawk_runner_thread_.joinable()) {
        error_message = "Only a single benchmark session is allowed at a time.";
        break;
      } else {
        nighthawk_runner_thread_ = std::thread(&ServiceImpl::NighthawkRunner, this, request);
      }
      break;
    case nighthawk::client::SendCommandRequest_CommandType::SendCommandRequest_CommandType_kUpdate:
      error_message = "Configuration updates are not supported yet.";
      break;
    default:
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  if (nighthawk_runner_thread_.joinable()) {
    nighthawk_runner_thread_.join();
  }
  EmitResponses(stream, error_message);
  if (error_message.empty()) {
    return grpc::Status::OK;
  }
  ENVOY_LOG(error, "One or more errors processing grpc request stream: {}", error_message);
  return grpc::Status(grpc::StatusCode::INTERNAL, fmt::format("Error: {}", error_message));
}

} // namespace Client
} // namespace Nighthawk