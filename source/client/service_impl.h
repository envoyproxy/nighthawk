#pragma once
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/client/service.grpc.pb.h"
#include "api/request_source/service.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <future>
#include <memory>

#include "external/envoy/source/common/common/lock_guard.h"
#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/thread.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/exe/process_wide.h"

#include "nighthawk/client/process.h"
#include "nighthawk/common/request_source.h"

namespace Nighthawk {
namespace Client {

/**
 * Implements Nighthawk's gRPC service. This service allows load generation to be
 * controlled by gRPC clients.
 */
class ServiceImpl final : public nighthawk::client::NighthawkService::Service,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  /**
   * Constructs a new ServiceImpl instance
   */
  ServiceImpl() : process_wide_(std::make_shared<Envoy::ProcessWide>()) {
    logging_context_ = std::make_unique<Envoy::Logger::Context>(
        spdlog::level::from_str("info"), "[%T.%f][%t][%L] %v", log_lock_, false);
  }

  ServiceImpl(std::unique_ptr<Envoy::Logger::Context>&& logging_context)
      : process_wide_(std::make_shared<Envoy::ProcessWide>()) {
    logging_context_ = std::move(logging_context);
  }

  grpc::Status
  ExecutionStream(grpc::ServerContext* context,
                  grpc::ServerReaderWriter<nighthawk::client::ExecutionResponse,
                                           nighthawk::client::ExecutionRequest>* stream) override;

private:
  void handleExecutionRequest(const nighthawk::client::ExecutionRequest& request);
  void writeResponse(const nighthawk::client::ExecutionResponse& response);
  grpc::Status finishGrpcStream(const bool success, absl::string_view description = "");

  std::unique_ptr<Envoy::Logger::Context> logging_context_;
  std::shared_ptr<Envoy::ProcessWide> process_wide_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  Envoy::Thread::MutexBasicLockable log_lock_;
  grpc::ServerReaderWriter<nighthawk::client::ExecutionResponse,
                           nighthawk::client::ExecutionRequest>* stream_;
  std::future<void> future_;
  Envoy::Thread::MutexBasicLockable lock_;
  // Set to true if there is a benchmark already in progress.
  // Nighthawk only supports a single benchmark at a time.
  bool benchmark_in_progress_ ABSL_GUARDED_BY(lock_);
};

/**
 * Dummy implementation of our request-source gRPC service definition, for testing and experimental
 * purposes.
 */
class RequestSourceServiceImpl final
    : public nighthawk::request_source::NighthawkRequestSourceService::Service,
      public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  /**
   * Constructs a new RequestSourceServiceImpl instance.
   */
  RequestSourceServiceImpl() {
    logging_context_ = std::make_unique<Envoy::Logger::Context>(
        spdlog::level::from_str("info"), "[%T.%f][%t][%L] %v", log_lock_, false);
  }

  grpc::Status RequestStream(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<nighthawk::request_source::RequestStreamResponse,
                               nighthawk::request_source::RequestStreamRequest>* stream) override;

private:
  std::unique_ptr<Envoy::Logger::Context> logging_context_;
  Envoy::Thread::MutexBasicLockable log_lock_;
  RequestSourcePtr createStaticEmptyRequestSource(const uint32_t amount);
};

} // namespace Client
} // namespace Nighthawk
