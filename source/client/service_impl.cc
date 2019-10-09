#include "client/service_impl.h"

#include <grpc++/grpc++.h>

#include "common/request_source_impl.h"

#include "client/client.h"
#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

void ServiceImpl::handleExecutionRequest(const nighthawk::client::ExecutionRequest& request) {
  std::unique_ptr<Envoy::Thread::LockGuard> busy_lock;
  {
    // Lock accepted_lock, in case we get here before accepted_event_.wait() is entered.
    auto accepted_lock = std::make_unique<Envoy::Thread::LockGuard>(accepted_lock_);
    // Acquire busy_lock_, and signal that we did so, allowing the service to continue
    // processing inbound requests on the stream.
    busy_lock = std::make_unique<Envoy::Thread::LockGuard>(busy_lock_);
    accepted_event_.notifyOne();
  }

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
  process.shutdown();
  // We release before writing the response to avoid a race with the client's follow up request
  // coming in before we release the lock, which would lead up to us declining service when
  // we should not.
  busy_lock.reset();
  writeResponse(response);
}

void ServiceImpl::writeResponse(const nighthawk::client::ExecutionResponse& response) {
  ENVOY_LOG(debug, "Write response: {}", response.DebugString());
  if (!stream_->Write(response)) {
    ENVOY_LOG(warn, "Failed to write response to the stream");
  }
}

::grpc::Status ServiceImpl::finishGrpcStream(const bool success, absl::string_view description) {
  // We may get here while there's still an active future in-flight in the error-paths.
  // Allow it to wrap up and put it's response on the stream before finishing the stream.
  if (future_.valid()) {
    future_.wait();
  }
  stream_ = nullptr;
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
    ENVOY_LOG(debug, "Read ExecutionRequest data {}", request.DebugString());
    if (request.has_start_request()) {
      // If busy_lock_ is held we can't start a new benchmark run because one is active already.
      if (busy_lock_.tryLock()) {
        busy_lock_.unlock();
        Envoy::Thread::LockGuard accepted_lock(accepted_lock_);
        // We pass in std::launch::async to avoid lazy evaluation, as we want this to run
        // asap. See: https://en.cppreference.com/w/cpp/thread/async
        future_ = std::future<void>(
            std::async(std::launch::async, &ServiceImpl::handleExecutionRequest, this, request));
        // Block until the thread associated to the future has acquired busy_lock_
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

namespace {
void addHeader(envoy::api::v2::core::HeaderMap* map, absl::string_view key,
               absl::string_view value) {
  auto* request_header = map->add_headers();
  request_header->set_key(std::string(key));
  request_header->set_value(std::string(value));
}
} // namespace

RequestSourcePtr ServiceImpl::createStaticEmptyRequestSource(const uint32_t amount) {
  Envoy::Http::HeaderMapPtr header = std::make_unique<Envoy::Http::HeaderMapImpl>();
  header->addCopy(Envoy::Http::LowerCaseString("x-from-remote-request-source"), "1");
  return std::make_unique<StaticRequestSourceImpl>(std::move(header), amount);
}

::grpc::Status ServiceImpl::HeaderStream(
    ::grpc::ServerContext* /*context*/,
    ::grpc::ServerReaderWriter<::nighthawk::client::HeaderStreamResponse,
                               ::nighthawk::client::HeaderStreamRequest>* stream) {
  nighthawk::client::HeaderStreamRequest request;
  bool ok = true;
  while (stream->Read(&request)) {
    ENVOY_LOG(trace, "Inbound HeaderStreamRequest {}", request.DebugString());

    // TODO(oschaaf): this is useful for integration testing purposes, but sending
    // these nearly empty headers will basically be a near no-op (note that the client will merge
    // headers we send here into into own header configuration). The client can be configured to
    // connect to a custom grpc service as a remote data source instead of this one, and its workers
    // will comply. That in itself may be useful. But we could offer the following features here:
    // 1. Yet another remote header source, so we balance to-be-replayed headers over workers
    //    and only have a single stream to a remote service here.
    // 2. Read a and dispatch a header stream from disk.
    auto request_source = createStaticEmptyRequestSource(request.amount());
    auto request_generator = request_source->get();
    RequestPtr request;
    while (ok && (request = request_generator()) != nullptr) {
      HeaderMapPtr headers = request->header();
      nighthawk::client::HeaderStreamResponse response;
      auto* request_headers = response.mutable_headers();
      headers->iterate(
          [](const Envoy::Http::HeaderEntry& header,
             void* context) -> Envoy::Http::HeaderMap::Iterate {
            addHeader(static_cast<envoy::api::v2::core::HeaderMap*>(context),
                      header.key().getStringView(), header.value().getStringView());
            return Envoy::Http::HeaderMap::Iterate::Continue;
          },
          request_headers);
      ok = ok && stream->Write(response);
      response.set_content_length(1);
      auto* expectation = response.add_expectations();
      expectation->set_input_header(":status");
      expectation->set_name("response_code_is_ok");
    }
    if (!ok) {
      ENVOY_LOG(error, "Failed to send the complete set of replay data.");
      break;
    }
  }
  ENVOY_LOG(trace, "Finishing stream");
  return ok ? grpc::Status::OK : grpc::Status(grpc::StatusCode::INTERNAL, std::string("error"));
}

} // namespace Client
} // namespace Nighthawk