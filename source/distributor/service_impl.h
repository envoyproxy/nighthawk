#pragma once
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif

#include <tuple>

#include "api/distributor/distributor.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/statusor.h"

#include "nighthawk/common/nighthawk_service_client.h"

namespace Nighthawk {

class NighthawkDistributorServiceImpl final
    : public nighthawk::NighthawkDistributor::Service,
      public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
public:
  NighthawkDistributorServiceImpl(std::unique_ptr<NighthawkServiceClient> service_client)
      : service_client_(std::move(service_client)) {}
  grpc::Status DistributedRequestStream(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<nighthawk::DistributedResponse, nighthawk::DistributedRequest>*
          stream) override;

private:
  grpc::Status validateRequest(const nighthawk::DistributedRequest& request) const;
  std::tuple<grpc::Status, nighthawk::DistributedResponse>
  handleRequest(const nighthawk::DistributedRequest& request) const;
  absl::StatusOr<nighthawk::client::ExecutionResponse>
  handleExecutionRequest(const envoy::config::core::v3::Address& service,
                         const nighthawk::client::ExecutionRequest& request) const;

  std::unique_ptr<NighthawkServiceClient> service_client_;
};

} // namespace Nighthawk
