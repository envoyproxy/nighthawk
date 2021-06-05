#pragma once
#include "envoy/common/pure.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/distributor/distributor.grpc.pb.h"

namespace Nighthawk {

/**
 * Interface of a gRPC distributor service client.
 */
class NighthawkDistributorClient {
public:
  virtual ~NighthawkDistributorClient() = default;

  /**
   * Propagate messages to one or more other services for handling.
   *
   * @param nighthawk_distributor_stub Used to open a channel to the distributor service.
   * @param distributed_request Provide the message that the distributor service should propagate.
   * @return absl::StatusOr<::nighthawk::DistributedResponse> Either a status indicating failure, or
   * a DistributedResponse upon success.
   */
  virtual absl::StatusOr<::nighthawk::DistributedResponse>
  DistributedRequest(nighthawk::NighthawkDistributor::StubInterface& nighthawk_distributor_stub,
                     const nighthawk::DistributedRequest& distributed_request) const PURE;
};

} // namespace Nighthawk
