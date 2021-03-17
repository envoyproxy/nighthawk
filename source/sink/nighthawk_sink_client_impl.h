#pragma once

#include "nighthawk/sink/nighthawk_sink_client.h"

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
  absl::StatusOr<nighthawk::StoreExecutionResponse> StoreExecutionResponseStream(
      nighthawk::NighthawkSink::StubInterface& nighthawk_sink_stub,
      const nighthawk::StoreExecutionRequest& store_execution_request) const override;

  absl::StatusOr<nighthawk::SinkResponse>
  SinkRequestStream(nighthawk::NighthawkSink::StubInterface& nighthawk_sink_stub,
                    const nighthawk::SinkRequest& sink_request) const override;
};

} // namespace Nighthawk
