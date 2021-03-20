#pragma once

#include "nighthawk/common/nighthawk_distributor_client.h"

namespace Nighthawk {

class NighthawkDistributorClientImpl : public NighthawkDistributorClient {
public:
  absl::StatusOr<::nighthawk::DistributedResponse>
  DistributedRequest(nighthawk::NighthawkDistributor::StubInterface& nighthawk_distributor_stub,
                     const nighthawk::DistributedRequest& distributed_request) const override;
};

} // namespace Nighthawk
