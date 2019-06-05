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
  void Push(T element) {
    Envoy::Thread::LockGuard lock(mutex_);
    queue_.push(element);
    condvar_.notifyOne();
  }

  T Pop() {
    Envoy::Thread::LockGuard lock(mutex_);
    while (queue_.empty()) {
      condvar_.wait(mutex_);
    }
    T element = queue_.front();
    queue_.pop();
    return element;
  }

  bool IsEmpty() {
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
  ServiceProcessResult(const nighthawk::client::SendCommandResponse& response, bool success)
      : response_(response), success_(success) {}

  nighthawk::client::SendCommandResponse response() const { return response_; }
  bool succes() const { return success_; }

private:
  const nighthawk::client::SendCommandResponse response_;
  bool success_;
};

class ServiceImpl final : public nighthawk::client::NighthawkService::Service,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  virtual ::grpc::Status
  SendCommand(::grpc::ServerContext* context,
              ::grpc::ServerReaderWriter<::nighthawk::client::SendCommandResponse,
                                         ::nighthawk::client::SendCommandRequest>* stream) override;

private:
  void NighthawkRunner(nighthawk::client::SendCommandRequest start_request);
  void EmitResponses(::grpc::ServerReaderWriter<::nighthawk::client::SendCommandResponse,
                                                ::nighthawk::client::SendCommandRequest>* stream,
                     std::string& error_messages);

  BlockingQueue<ServiceProcessResult> response_queue_;
  std::thread nighthawk_runner_thread_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  ProcessPtr process_;
};

} // namespace Client
} // namespace Nighthawk