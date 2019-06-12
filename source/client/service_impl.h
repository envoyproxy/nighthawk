#pragma once
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/client/service.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "common/common/logger.h"
#include "common/common/thread.h"

#include "nighthawk/client/process.h"

namespace Nighthawk {
namespace Client {

class ServiceProcessResult {
public:
  ServiceProcessResult(nighthawk::client::ExecutionResponse response,
                       absl::string_view error_message)
      : response_(std::move(response)), error_message_(std::string(error_message)) {}

  nighthawk::client::ExecutionResponse response() const { return response_; }
  bool success() const { return error_message_.empty(); }
  std::string error_message() const { return error_message_; }

private:
  const nighthawk::client::ExecutionResponse response_;
  std::string error_message_;
};

class ServiceImpl final : public nighthawk::client::NighthawkService::Service,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  ::grpc::Status
  sendCommand(::grpc::ServerContext* context,
              ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                                         ::nighthawk::client::ExecutionRequest>* stream) override;

private:
  void handleExecutionRequest(const nighthawk::client::ExecutionRequest& start_request);
  void emitResponses(::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                                                ::nighthawk::client::ExecutionRequest>* stream,
                     std::string& error_messages);

  std::list<ServiceProcessResult> response_queue_;
  std::thread nighthawk_runner_thread_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  Envoy::Thread::MutexBasicLockable log_lock_;
  bool running_{};
};

} // namespace Client
} // namespace Nighthawk