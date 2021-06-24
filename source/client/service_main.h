#pragma once

#include <grpc++/grpc++.h>

#include <thread>
#include <vector>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/thread.h"

#include "api/client/service.pb.h"

#include "source/client/service_impl.h"
#include "source/common/signal_handler.h"

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
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Service> service_;
  std::unique_ptr<grpc::Server> server_;
  int listener_port_{-1};
  std::string listener_bound_address_;
  std::string listener_output_path_;
  SignalHandlerPtr signal_handler_;
};

} // namespace Client
} // namespace Nighthawk
