#include "source/common/signal_handler.h"

#include <csignal>

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/common/macros.h"

namespace Nighthawk {

namespace {
std::function<void(int)> signal_handler_delegate;
// The delegate is cleared at the very end of ~SignalHandler(), after the dispositions have
// been restored; the null check is defense-in-depth for the small window where a signal was
// already dispatched into this handler while the destructor is swapping dispositions.
void signal_handler(int signal) {
  if (signal_handler_delegate) {
    signal_handler_delegate(signal);
  }
}
} // namespace

SignalHandler::SignalHandler(const std::function<void()>& signal_callback) {
  pipe_fds_.resize(2);
  // The shutdown thread will be notified of by our signal handler and take it from there.
  RELEASE_ASSERT(pipe(pipe_fds_.data()) == 0, "pipe failed");

  signal_handler_delegate = [this](int) { onSignal(); };
  // Save the dispositions we replace so we can restore them after the first handled signal
  // (second-signal escalation) and upon destruction. This happens before the shutdown
  // thread is spawned so the thread is guaranteed to observe the saved values.
  previous_sigterm_handler_ = signal(SIGTERM, signal_handler);
  previous_sigint_handler_ = signal(SIGINT, signal_handler);
  RELEASE_ASSERT(previous_sigterm_handler_ != SIG_ERR, "signal(SIGTERM) failed");
  RELEASE_ASSERT(previous_sigint_handler_ != SIG_ERR, "signal(SIGINT) failed");

  shutdown_thread_ = std::thread([this, signal_callback]() {
    int tmp;
    RELEASE_ASSERT(read(pipe_fds_[0], &tmp, sizeof(int)) >= 0, "read failed");
    RELEASE_ASSERT(close(pipe_fds_[0]) == 0, "read side close failed");
    RELEASE_ASSERT(close(pipe_fds_[1]) == 0, "write side close failed");
    pipe_fds_.clear();
    if (!destructing_) {
      // The first signal is about to be handled: fall back to the previous (typically
      // default) dispositions BEFORE invoking the callback, so that any further signal
      // gets the OS default action - giving the user a force-quit escalation path. This
      // must happen before the callback rather than after it: the callback may itself
      // block on the very shutdown sequence the user is trying to escape (e.g.
      // requestExecutionCancellation() contends on the lock held across the worker drain
      // loop in ProcessImpl::shutdown()), and a restore placed after it would never run
      // in exactly that case. Calling signal() from this (non-main) thread is fine:
      // POSIX specifies signal()/sigaction() as thread-safe (and async-signal-safe).
      signal(SIGTERM, previous_sigterm_handler_);
      signal(SIGINT, previous_sigint_handler_);
      signal_callback();
    }
  });
}

SignalHandler::~SignalHandler() {
  // Ordering rationale:
  // 1) Set destructing_ first, so that if the shutdown thread gets woken up below (or by a
  //    concurrently arriving signal) it will not invoke the callback mid-destruction.
  // 2) Restore the previous signal dispositions. From this point on no NEW invocations of
  //    the file-scope signal_handler() can start, so it becomes safe to tear down the
  //    state it depends on. (If the shutdown thread already restored them after handling
  //    a first signal, this re-installs the same values: harmless.)
  // 3) Wake the shutdown thread if no signal ever fired (initiateShutdown() writes to the
  //    pipe; it is a no-op when the thread already consumed a signal and closed the pipe)
  //    and join it, draining the only thread that runs the callback or touches the pipe.
  // 4) Only then clear signal_handler_delegate. Doing this any earlier could leave an
  //    installed handler pointing at an empty std::function. Inherent (accepted) race: a
  //    signal already dispatched into signal_handler() on another thread just before (2)
  //    may still be executing; keeping the delegate alive until after restore+join makes
  //    that window safe - the delegate only writes to the pipe, a no-op once closed.
  destructing_ = true;
  signal(SIGTERM, previous_sigterm_handler_);
  signal(SIGINT, previous_sigint_handler_);
  initiateShutdown();
  if (shutdown_thread_.joinable()) {
    shutdown_thread_.join();
  }
  signal_handler_delegate = nullptr;
}

void SignalHandler::initiateShutdown() {
  if (pipe_fds_.size() == 2) {
    const int tmp = 0;
    RELEASE_ASSERT(write(pipe_fds_[1], &tmp, sizeof(int)) == sizeof(int), "write failed");
  }
}

void SignalHandler::onSignal() { initiateShutdown(); }

} // namespace Nighthawk
