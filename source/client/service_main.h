#pragma once

#include <grpc++/grpc++.h>

#include <thread>
#include <vector>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/thread.h"

#include "api/client/service.pb.h"

#include "client/service_impl.h"

#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

class ServiceMain : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  ServiceMain(int argc, const char** argv);
  void start();
  /**
   * Can be used to block while waiting for the server to exit. Registers to SIGTERM/SIGINT and will
   * commence shutdown of the gRPC service upon reception of those signals.
   */
  void wait();

  /**
   * Can be used to shut down the server.
   */
  void shutdown();

  static std::string appendDefaultPortIfNeeded(absl::string_view host_and_maybe_port);

private:
  /**
   * Notifies the thread responsible for shutting down the server that it is time to do so, if
   * needed. Safe to use in signal handling, and non-blocking.
   */
  void initiateShutdown();

  /**
   * Fires on signal reception.
   */
  void onSignal();

  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Service> service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub_;
  int listener_port_{-1};
  std::string listener_bound_address_;
  std::string listener_output_path_;
  // Signal handling needs to be lean so we can't directly initiate shutdown while handling a
  // signal. Therefore we write a bite to a this pipe to propagate signal reception. Subsequently,
  // the read side will handle the actual shut down of the gRPC service without having to worry
  // about signal-safety.
  std::vector<int> pipe_fds_;
  std::thread shutdown_thread_;
};

} // namespace Client
} // namespace Nighthawk
