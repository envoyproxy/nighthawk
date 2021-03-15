#pragma once
#include "envoy/common/pure.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/distributor/distributor.grpc.pb.h"

namespace Nighthawk {

class NighthawkDistributorClient {
public:
  virtual ~NighthawkDistributorClient() = default;

  virtual absl::StatusOr<::nighthawk::DistributedResponse>
  DistributedRequest(nighthawk::NighthawkDistributor::StubInterface& nighthawk_distributor_stub,
                     const nighthawk::DistributedRequest& distributed_request) const PURE;
};

} // namespace Nighthawk
