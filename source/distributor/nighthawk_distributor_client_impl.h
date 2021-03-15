#include "nighthawk/common/nighthawk_distributor_client.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {

class NighthawkDistributorClientImpl : public NighthawkDistributorClient {
public:
  absl::StatusOr<::nighthawk::DistributedResponse>
  DistributedRequest(nighthawk::NighthawkDistributor::StubInterface& nighthawk_distributor_stub,
                     const nighthawk::DistributedRequest& distributed_request) const override;
};

} // namespace Nighthawk
