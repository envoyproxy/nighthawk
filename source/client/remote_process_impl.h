#pragma once

#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"
#include "nighthawk/client/process.h"
#include "nighthawk/common/nighthawk_service_client.h"

#include "external/envoy/source/common/common/logger.h"

#include "api/client/service.grpc.pb.h"

namespace Nighthawk {
namespace Client {

/**
 * Will delegate execution to a remote nighthawk_service using gRPC.
 */
class RemoteProcessImpl : public Process, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * @param options Options to send to the remote nighthawk service, as well as
   * containing information to connect to it (which won't be forwarded).
   * @param stub Stub that will be used to communicate with the remote
   * gRPC server.
   */
  RemoteProcessImpl(const Options& options, nighthawk::client::NighthawkService::Stub& stub);
  /**
   * @param collector Collects the output from the remote nighthawk service.
   * @return OK if the remote execution should be considered successful. Unsuccessful execution
   * will log available error details and return an error status.
   */
  absl::Status run(OutputCollector& collector) override;
  /**
   * Shuts down the service, a no-op in this implementation.
   */
  void shutdown() override{};

  bool requestExecutionCancellation() override;

private:
  const Options& options_;
  const std::unique_ptr<NighthawkServiceClient> service_client_;
  nighthawk::client::NighthawkService::Stub& stub_;
};

} // namespace Client
} // namespace Nighthawk
