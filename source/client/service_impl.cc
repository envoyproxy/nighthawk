#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "client/client.h"
#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

void ServiceImpl::handleExecutionRequest(const nighthawk::client::ExecutionRequest& request) {
  OptionsPtr options;
  try {
    options = std::make_unique<OptionsImpl>(request.start_request().options());
  } catch (Envoy::EnvoyException exception) {
    response_queue_.push_back(ServiceProcessResult({}, exception.what()));
    return;
  }

  auto logging_context = std::make_unique<Envoy::Logger::Context>(
      spdlog::level::from_str(options->verbosity()), "[%T.%f][%t][%L] %v", log_lock_);
  process_ = std::make_unique<ProcessImpl>(*options, time_system_);
  OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
  auto formatter = output_format_factory.create();
  bool success = process_->run(*formatter);
  nighthawk::client::ExecutionResponse response;
  *(response.mutable_output()) = formatter->toProto();
  response_queue_.emplace_back(ServiceProcessResult(response, success ? "" : "Unkown failure"));
  process_.reset();
}

void ServiceImpl::emitResponses(
    ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                               ::nighthawk::client::ExecutionRequest>* stream,
    std::string& error_messages) {
  for (const auto result : response_queue_) {
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
// TODO(oschaaf): add some logging to this.
// TODO(oschaaf): unit-test Process, create MockProcess & use in service_test.cc / client_test.cc
// TODO(oschaaf): finish option validations, should we add defaults?
// TODO(oschaaf): aggregate the logs and forward them in the grpc result-response.
::grpc::Status ServiceImpl::sendCommand(
    ::grpc::ServerContext* /*context*/,
    ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                               ::nighthawk::client::ExecutionRequest>* stream) {
  std::string error_message;
  nighthawk::client::ExecutionRequest request;

  while (stream->Read(&request)) {
    if (request.has_start_request()) {
      if (running_) {
        error_message = "Only a single benchmark session is allowed at a time.";
        break;
      } else {
        running_ = true;
        nighthawk_runner_thread_ = std::thread(&ServiceImpl::handleExecutionRequest, this, request);
      }
    } else if (request.has_update_request()) {
      error_message = "Configuration updates are not supported yet.";
    } else {
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  if (running_ && nighthawk_runner_thread_.joinable()) {
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