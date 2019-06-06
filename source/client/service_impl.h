#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "api/client/service.grpc.pb.h"

#pragma clang diagnostic pop

#include <queue>

#include "common/common/lock_guard.h"
#include "common/common/logger.h"
#include "common/common/thread.h"

#include "nighthawk/client/process.h"

namespace Nighthawk {
namespace Client {

// TODO(oschaaf): add tests & move into own file.
template <class T> class BlockingQueue {
public:
  void push(T element) {
    Envoy::Thread::LockGuard lock(mutex_);
    queue_.push(element);
    condvar_.notifyOne();
  }

  T pop() {
    Envoy::Thread::LockGuard lock(mutex_);
    while (queue_.empty()) {
      condvar_.wait(mutex_);
    }
    T element = queue_.front();
    queue_.pop();
    return element;
  }

  bool isEmpty() {
    Envoy::Thread::LockGuard lock(mutex_);
    return queue_.empty();
  }

private:
  Envoy::Thread::MutexBasicLockable mutex_;
  Envoy::Thread::CondVar condvar_;
  std::queue<T> queue_ GUARDED_BY(mutex_);
};

class ServiceProcessResult {
public:
  ServiceProcessResult(const nighthawk::client::ExecutionResponse& response,
                       absl::string_view error_message)
      : response_(response), error_message_(std::string(error_message)) {}

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
  virtual ::grpc::Status
  sendCommand(::grpc::ServerContext* context,
              ::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                                         ::nighthawk::client::ExecutionRequest>* stream) override;

private:
  void nighthawkRunner(nighthawk::client::ExecutionRequest start_request);
  void emitResponses(::grpc::ServerReaderWriter<::nighthawk::client::ExecutionResponse,
                                                ::nighthawk::client::ExecutionRequest>* stream,
                     std::string& error_messages);

  BlockingQueue<ServiceProcessResult> response_queue_;
  std::thread nighthawk_runner_thread_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  ProcessPtr process_ GUARDED_BY(mutex_);
  Envoy::Thread::MutexBasicLockable mutex_;
};

} // namespace Client
} // namespace Nighthawk