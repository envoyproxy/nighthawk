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

class ServiceImpl final : public nighthawk::client::NighthawkService::Service,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  ::grpc::Status
  sendCommand(::grpc::ServerContext* context,
              ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                                         ::nighthawk::client::ExecutionRequest>* stream) override;

private:
  void handleExecutionRequest(const nighthawk::client::ExecutionRequest& request);
  void collectErrorsFromHistory(std::list<std::string>& error_messages) const;
  void writeResponseAndFinish(const nighthawk::client::ExecutionResponse& response);
  void waitForRunnerThreadCompletion();

  std::list<nighthawk::client::ExecutionResponse> response_history_;
  std::thread runner_thread_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  Envoy::Thread::MutexBasicLockable log_lock_;
  std::atomic<bool> running_{};
  ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                             ::nighthawk::client::ExecutionRequest>* stream_;
};

} // namespace Client
} // namespace Nighthawk