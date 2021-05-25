#pragma once

#include <tuple>

#include "nighthawk/common/nighthawk_service_client.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/statusor.h"

#include "api/distributor/distributor.grpc.pb.h"

namespace Nighthawk {

/**
 * Implements a real-world distributor gRPC service.
 */
class NighthawkDistributorServiceImpl final
    : public nighthawk::NighthawkDistributor::Service,
      public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * Construct a new gRPC distributor service instance.
   *
   * @param service_client gRPC client that will be used to communicate with Nighthawk's load
   * generator services.
   */
  NighthawkDistributorServiceImpl(std::unique_ptr<NighthawkServiceClient> service_client)
      : service_client_(std::move(service_client)) {}

  grpc::Status DistributedRequestStream(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<nighthawk::DistributedResponse, nighthawk::DistributedRequest>*
          stream) override;

private:
  std::tuple<grpc::Status, nighthawk::DistributedResponse>
  handleRequest(const nighthawk::DistributedRequest& request) const;
  absl::StatusOr<nighthawk::client::ExecutionResponse>
  handleExecutionRequest(const envoy::config::core::v3::Address& service,
                         const nighthawk::client::ExecutionRequest& request) const;

  std::unique_ptr<NighthawkServiceClient> service_client_;
};

} // namespace Nighthawk
