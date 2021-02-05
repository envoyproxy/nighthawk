#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

/**
 * Callback definition for providing a delegate that should be executed after a signal
 * is observed.
 */
using SignalCallback = std::function<void()>;

/**
 * Utility class for handling TERM and INT signals. Allows wiring up a callback that
 * should be invoked upon signal reception. This callback implementation does not have to be
 * signal safe, as a different thread is used to fire it.
 * NOTE: Only the first observed signal will result in the callback being invoked.
 * WARNING: only a single instance should be active at any given time in a process, and
 * the responsibility for not breaking this rule is not enforced at this time.
 *
 * Example usage:
 *
 * Process p;
 * {
 *   // Signals will be handled while in this scope.
 *   // The provided callback will call cancel(), gracefully terminating
 *   // execution.
 *   auto s = SignalHandler([&p]() { log("cancelling!"); p->cancel(); });
 *   p->executeInfinitelyOrUntilCancelled();
 * }
 *
 */
class SignalHandler final : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * Constructs a new SignalHandler instance.
   * WARNING: Only a single instance is allowed to be active process-wide at any given time.
   * @param signal_callback will be invoked after the first signal gets caught. Does not need to be
   * signal-safe.
   */
  SignalHandler(const SignalCallback& signal_callback);

  // Not copyable or movable.
  SignalHandler(SignalHandler const&) = delete;
  void operator=(SignalHandler const&) = delete;
  ~SignalHandler();

private:
  /**
   * Fires on signal reception.
   */
  void onSignal();

  /**
   * Notifies the thread responsible for shutting down the server that it is time to do so, if
   * needed. Safe to use in signal handling, and non-blocking.
   */
  void initiateShutdown();

  std::thread shutdown_thread_;

  // Signal handling needs to be lean so we can't directly initiate shutdown while handling a
  // signal. Therefore we write a bite to a this pipe to propagate signal reception. Subsequently,
  // the read side will handle the actual shut down of the gRPC service without having to worry
  // about signal-safety.
  std::vector<int> pipe_fds_;
  bool destructing_{false};
};

using SignalHandlerPtr = std::unique_ptr<SignalHandler>;

} // namespace Nighthawk
