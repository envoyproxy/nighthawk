#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "client/client.h"
#include "client/options_impl.h"

#include "api/client/options.pb.validate.h"

namespace Nighthawk {
namespace Client {

void ServiceImpl::nighthawkRunner(nighthawk::client::ExecutionRequest request) {
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
    nighthawk::client::ExecutionResponse response;
    response_queue_.push(ServiceProcessResult(response, exception.what()));
    process_.reset();
    return;
  }

  OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
  auto formatter = output_format_factory.create();
  bool success = process_->run(*formatter);
  nighthawk::client::ExecutionResponse response;
  *(response.mutable_output()) = formatter->toProto();
  response_queue_.push(ServiceProcessResult(response, success ? "" : "Unkown failure"));
  process_.reset();
}

void ServiceImpl::emitResponses(
    ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                               ::nighthawk::client::ExecutionRequest>* stream,
    std::string& error_messages) {

  while (!response_queue_.isEmpty()) {
    auto result = response_queue_.pop();
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
::grpc::Status ServiceImpl::sendCommand(
    ::grpc::ServerContext* /*context*/,
    ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                               ::nighthawk::client::ExecutionRequest>* stream) {
  std::string error_message = "";

  nighthawk::client::ExecutionRequest request;
  while (stream->Read(&request)) {
    Envoy::Thread::LockGuard lock(mutex_);
    switch (request.command_type()) {
    case nighthawk::client::ExecutionRequest_CommandType::ExecutionRequest_CommandType_START:
      if (nighthawk_runner_thread_.joinable()) {
        error_message = "Only a single benchmark session is allowed at a time.";
        break;
      } else {
        nighthawk_runner_thread_ = std::thread(&ServiceImpl::nighthawkRunner, this, request);
      }
      break;
    case nighthawk::client::ExecutionRequest_CommandType::ExecutionRequest_CommandType_UPDATE:
      error_message = "Configuration updates are not supported yet.";
      break;
    default:
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  if (nighthawk_runner_thread_.joinable()) {
    nighthawk_runner_thread_.join();
  }
  emitResponses(stream, error_message);
  if (error_message.empty()) {
    return grpc::Status::OK;
  }
  ENVOY_LOG(error, "One or more errors processing grpc request stream: {}", error_message);
  return grpc::Status(grpc::StatusCode::INTERNAL, fmt::format("Error: {}", error_message));
}

} // namespace Client
} // namespace Nighthawk