#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

using SignalCallback = std::function<void()>;

class SignalHandler final : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  SignalHandler(const SignalCallback& signal_callback);
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
};

using SignalHandlerPtr = std::unique_ptr<SignalHandler>;

} // namespace Nighthawk