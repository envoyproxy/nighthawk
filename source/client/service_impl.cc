#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "client/client.h"
#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

Envoy::Thread::MutexBasicLockable ServiceImpl::global_lock_;

void ServiceImpl::handleExecutionRequest(const nighthawk::client::ExecutionRequest& request) {
  auto accepted_lock = std::make_unique<Envoy::Thread::LockGuard>(accepted_lock_);
  auto busy_lock = std::make_unique<Envoy::Thread::LockGuard>(busy_lock_);
  accepted_event_.notifyOne();
  accepted_lock.reset();

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
    ProcessImpl process(*options, time_system_);
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
  busy_lock.reset();
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
  Envoy::Thread::TryLockGuard service_lock(global_lock_);
  if (!service_lock.tryLock()) {
    stream->Read(&request);
    return finishGrpcStream(false, "Another client is performing a benchmark.");
  }

  stream_ = stream;

  while (stream->Read(&request)) {
    ENVOY_LOG(debug, "Read ExecutionRequest data");
    if (request.has_start_request()) {
      if (busy_lock_.tryLock()) {
        busy_lock_.unlock();
        // We pass in std::launch::async to avoid lazy evaluation, as we want this to run
        // asap. See: https://en.cppreference.com/w/cpp/thread/async
        Envoy::Thread::LockGuard accepted_lock(accepted_lock_);
        future_ = std::future<void>(
            std::async(std::launch::async, &ServiceImpl::handleExecutionRequest, this, request));
        accepted_event_.wait(accepted_lock_);
      } else {
        return finishGrpcStream(false, "Only a single benchmark session is allowed at a time.");
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