#include "nighthawk/common/nighthawk_sink_client.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {

/**
 * Implements a the gRPC sink client interface.
 *
 * This class is stateless and may be called from multiple threads. Furthermore, the same gRPC stub
 * is safe to use from multiple threads simultaneously.
 */
class NighthawkSinkClientImpl : public NighthawkSinkClient {
public:
  absl::StatusOr<nighthawk::client::StoreExecutionResponse> StoreExecutionResponseStream(
      nighthawk::client::NighthawkSink::StubInterface* nighthawk_sink_stub,
      const nighthawk::client::StoreExecutionRequest& store_execution_request) const override;

  absl::StatusOr<nighthawk::client::SinkResponse>
  SinkRequestStream(nighthawk::client::NighthawkSink::StubInterface& nighthawk_sink_stub,
                    const nighthawk::client::SinkRequest& sink_request) const override;
};

} // namespace Nighthawk
