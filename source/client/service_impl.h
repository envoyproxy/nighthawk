#pragma once
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/client/service.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <future>

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/thread.h"
#include "external/envoy/source/common/event/real_time_system.h"

#include "nighthawk/client/process.h"

namespace Nighthawk {
namespace Client {

class ServiceImpl final : public nighthawk::client::NighthawkService::Service,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  ::grpc::Status ExecutionStream(
      ::grpc::ServerContext* context,
      ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                                 ::nighthawk::client::ExecutionRequest>* stream) override;

private:
  void handleExecutionRequest(const nighthawk::client::ExecutionRequest& request);
  void writeResponse(const nighthawk::client::ExecutionResponse& response);
  ::grpc::Status finishGrpcStream(const bool success, absl::string_view description = "");

  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  Envoy::Thread::MutexBasicLockable log_lock_;
  ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                             ::nighthawk::client::ExecutionRequest>* stream_;
  std::future<void> future_;
  static Envoy::Thread::MutexBasicLockable global_lock_;
  Envoy::Thread::MutexBasicLockable accepted_lock_;
  Envoy::Thread::MutexBasicLockable busy_lock_;
  Envoy::Thread::CondVar accepted_event_;
  std::atomic<bool> busy_{};
};

} // namespace Client
} // namespace Nighthawk