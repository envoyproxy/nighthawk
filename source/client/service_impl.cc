#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "client/client.h"
#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

void ServiceImpl::handleExecutionRequest(const nighthawk::client::ExecutionRequest& request) {
  RELEASE_ASSERT(running_, "running_ ought to be set");
  nighthawk::client::ExecutionResponse response;
  OptionsPtr options;
  try {
    options = std::make_unique<OptionsImpl>(request.start_request().options());
  } catch (Envoy::EnvoyException exception) {
    response.set_error_message(exception.what());
    writeResponseAndFinish(response);
    return;
  }

  ProcessImpl process(*options, time_system_);
  OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(options->verbosity()), "[%T.%f][%t][%L] %v", log_lock_);
  auto formatter = output_format_factory.create();
  if (process.run(*formatter)) {
    response.set_success(true);
  } else {
    response.set_error_message("Unkown failure");
  }
  *(response.mutable_output()) = formatter->toProto();
  writeResponseAndFinish(response);
}

void ServiceImpl::writeResponseAndFinish(const nighthawk::client::ExecutionResponse& response) {
  response_history_.emplace_back(response);
  if (!stream_->Write(response)) {
    ENVOY_LOG(warn, "Stream write failed");
  }
  running_ = false;
}

void ServiceImpl::collectErrorsFromHistory(std::list<std::string>& error_messages) const {
  for (const auto& result : response_history_) {
    if (!result.success()) {
      error_messages.push_back(result.error_message());
    }
  }
}

void ServiceImpl::waitForRunnerThreadCompletion() {
  if (runner_thread_.joinable()) {
    runner_thread_.join();
  }
  RELEASE_ASSERT(!running_, "running_ ought to be unset");
}

// TODO(oschaaf): implement a way to cancel test runs, and update rps config on the fly.
// TODO(oschaaf): unit-test Process, create MockProcess & use in service_test.cc / client_test.cc
// TODO(oschaaf): should we merge incoming request options with defaults?
// TODO(oschaaf): aggregate the client's logs and forward them in the grpc response.
::grpc::Status ServiceImpl::sendCommand(
    ::grpc::ServerContext* /*context*/,
    ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                               ::nighthawk::client::ExecutionRequest>* stream) {
  std::list<std::string> error_messages;
  nighthawk::client::ExecutionRequest request;
  stream_ = stream;
  while (stream->Read(&request)) {
    ENVOY_LOG(debug, "Read ExecutionRequest data: {}", request.DebugString());
    if (request.has_start_request()) {
      if (running_) {
        error_messages.push_back("Only a single benchmark session is allowed at a time.");
        break;
      } else {
        waitForRunnerThreadCompletion();
        running_ = true;
        runner_thread_ = std::thread(&ServiceImpl::handleExecutionRequest, this, request);
      }
    } else if (request.has_update_request()) {
      error_messages.push_back("Configuration updates are not supported yet.");
    } else {
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }
  waitForRunnerThreadCompletion();
  collectErrorsFromHistory(error_messages);
  if (error_messages.size() == 0) {
    return grpc::Status::OK;
  }
  std::string error_message = "";
  for (const auto& error : error_messages) {
    error_message = fmt::format("{}\n", error);
  }
  ENVOY_LOG(error, "One or more errors processing grpc request stream: {}", error_message);
  return grpc::Status(grpc::StatusCode::INTERNAL, fmt::format("Error: {}", error_message));
}

} // namespace Client
} // namespace Nighthawk