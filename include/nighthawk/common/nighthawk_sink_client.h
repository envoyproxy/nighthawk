#pragma once
#include "envoy/common/pure.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {

/**
 * Interface of a gRPC sink service client.
 */
class NighthawkSinkClient {
public:
  virtual ~NighthawkSinkClient() = default;

  /**
   * @brief Store an execution response.
   *
   * @param nighthawk_sink_stub Used to open a channel to the sink service.
   * @param store_execution_request Provide the message that the sink should store.
   * @return absl::StatusOr<nighthawk::client::StoreExecutionResponse>
   */
  virtual absl::StatusOr<nighthawk::client::StoreExecutionResponse> StoreExecutionResponseStream(
      nighthawk::client::NighthawkSink::StubInterface* nighthawk_sink_stub,
      const nighthawk::client::StoreExecutionRequest& store_execution_request) const PURE;

  /**
   * Look up ExecutionResponse messages in the sink.
   *
   * @param nighthawk_sink_stub Used to open a channel to the sink service.
   * @param sink_request Provide the message that the sink should handle.
   * @return absl::StatusOr<nighthawk::client::SinkResponse> Either a status indicating failure, or
   * a SinkResponse upon success.
   */
  virtual absl::StatusOr<nighthawk::client::SinkResponse>
  SinkRequestStream(nighthawk::client::NighthawkSink::StubInterface& nighthawk_sink_stub,
                    const nighthawk::client::SinkRequest& sink_request) const PURE;
};

} // namespace Nighthawk
