#include "common/signal_handler.h"

#include <csignal>

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/common/macros.h"

namespace Nighthawk {

namespace {
std::function<void(int)> signal_handler_delegate;
void signal_handler(int signal) { signal_handler_delegate(signal); }
} // namespace

SignalHandler::SignalHandler(const std::function<void()>& signal_callback) {
  pipe_fds_.resize(2);
  // The shutdown thread will be notified of by our signal handler and take it from there.
  RELEASE_ASSERT(pipe(pipe_fds_.data()) == 0, "pipe failed");

  shutdown_thread_ = std::thread([this, signal_callback]() {
    int tmp;
    RELEASE_ASSERT(read(pipe_fds_[0], &tmp, sizeof(int)) >= 0, "read failed");
    RELEASE_ASSERT(close(pipe_fds_[0]) == 0, "read side close failed");
    RELEASE_ASSERT(close(pipe_fds_[1]) == 0, "write side close failed");
    pipe_fds_.clear();
    if (!destructing_) {
      signal_callback();
    }
  });

  signal_handler_delegate = [this](int) { onSignal(); };
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
}

SignalHandler::~SignalHandler() {
  destructing_ = true;
  initiateShutdown();
  if (shutdown_thread_.joinable()) {
    shutdown_thread_.join();
  }
}

void SignalHandler::initiateShutdown() {
  if (pipe_fds_.size() == 2) {
    const int tmp = 0;
    RELEASE_ASSERT(write(pipe_fds_[1], &tmp, sizeof(int)) == sizeof(int), "write failed");
  }
}

void SignalHandler::onSignal() { initiateShutdown(); }

} // namespace Nighthawk
