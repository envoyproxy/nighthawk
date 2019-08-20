#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "client/client.h"
#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

void ServiceImpl::handleExecutionRequest(const nighthawk::client::ExecutionRequest& request) {
  nighthawk::client::ExecutionResponse response;
  response.mutable_error_detail()->set_code(grpc::StatusCode::INTERNAL);

  OptionsPtr options;
  try {
    options = std::make_unique<OptionsImpl>(request.start_request().options());
  } catch (MalformedArgvException e) {
    response.mutable_error_detail()->set_message(e.what());
    writeResponse(response);
    return;
  }

  // We scope here because the ProcessImpl instance must be destructed before we write the response
  // and set running to false.
  {
    PlatformUtilImpl platform_util;
    ProcessImpl process(*options, time_system_, platform_util);
    OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
    auto logging_context = std::make_unique<Envoy::Logger::Context>(
        spdlog::level::from_str(
            nighthawk::client::Verbosity::VerbosityOptions_Name(options->verbosity())),
        "[%T.%f][%t][%L] %v", log_lock_);
    auto formatter = output_format_factory.create();
    if (process.run(*formatter)) {
      response.clear_error_detail();
      *(response.mutable_output()) = formatter->toProto();
    } else {
      response.mutable_error_detail()->set_message("Unknown failure");
    }
  }
  writeResponse(response);
}

void ServiceImpl::writeResponse(const nighthawk::client::ExecutionResponse& response) {
  busy_ = false;
  if (!stream_->Write(response)) {
    ENVOY_LOG(warn, "Stream write failed");
  }
}

::grpc::Status ServiceImpl::finishGrpcStream(const bool success, absl::string_view description) {
  if (future_.valid()) {
    future_.wait();
  }
  return success ? grpc::Status::OK
                 : grpc::Status(grpc::StatusCode::INTERNAL, std::string(description));
}

// TODO(oschaaf): implement a way to cancel test runs, and update rps config on the fly.
// TODO(oschaaf): unit-test Process, create MockProcess & use in service_test.cc / client_test.cc
// TODO(oschaaf): should we merge incoming request options with defaults?
// TODO(oschaaf): aggregate the client's logs and forward them in the grpc response.
::grpc::Status ServiceImpl::ExecutionStream(
    ::grpc::ServerContext* /*context*/,
    ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                               ::nighthawk::client::ExecutionRequest>* stream) {
  nighthawk::client::ExecutionRequest request;
  stream_ = stream;
  while (stream->Read(&request)) {
    ENVOY_LOG(debug, "Read ExecutionRequest data: {}", request.DebugString());
    if (request.has_start_request()) {
      // It is possible to receive a back-to-back request here, while the future that is associated
      // to our previous response is still active. We check the running_ flag to see if the previous
      // future has progressed in a state where we can do another one. This avoids the odd flake in
      // ServiceTest.BackToBackExecution.
      if (future_.valid() &&
          future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready && busy_) {
        return finishGrpcStream(false, "Only a single benchmark session is allowed at a time.");
      } else {
        busy_ = true;
        // We pass in std::launch::async to avoid lazy evaluation, as we want this to run
        // asap. See: https://en.cppreference.com/w/cpp/thread/async
        future_ = std::future<void>(
            std::async(std::launch::async, &ServiceImpl::handleExecutionRequest, this, request));
      }
    } else if (request.has_update_request() || request.has_cancellation_request()) {
      return finishGrpcStream(false, "Request is not supported yet.");
    } else {
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  return finishGrpcStream(true);
}

} // namespace Client
} // namespace Nighthawk