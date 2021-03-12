#include "nighthawk/common/nighthawk_sink_client.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

namespace Nighthawk {

/**
 * Implements a the gRPC sink client interface.
 *
 * This class is stateless and may be called from multiple threads. Furthermore, the same gRPC stub
 * is safe to use from multiple threads simultaneously.
 */
class NighthawkSinkClientImpl : public NighthawkSinkClient {
public:
  absl::StatusOr<nighthawk::sink::StoreExecutionResponse> StoreExecutionResponseStream(
      nighthawk::sink::NighthawkSink::StubInterface* nighthawk_sink_stub,
      const nighthawk::sink::StoreExecutionRequest& store_execution_request) const override;

  absl::StatusOr<nighthawk::sink::SinkResponse>
  SinkRequestStream(nighthawk::sink::NighthawkSink::StubInterface& nighthawk_sink_stub,
                    const nighthawk::sink::SinkRequest& sink_request) const override;
};

} // namespace Nighthawk
