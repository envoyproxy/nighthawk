#include "nighthawk/common/nighthawk_service_client.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {

/**
 * Real implementation of a helper that opens a channel with the gRPC stub, sends the input, and
 * translates the output or errors into a StatusOr.
 *
 * This class is stateless and may be called from multiple threads. Furthermore, the same gRPC stub
 * is safe to use from multiple threads simultaneously.
 */
class NighthawkServiceClientImpl : public NighthawkServiceClient {
public:
  absl::StatusOr<nighthawk::client::ExecutionResponse> PerformNighthawkBenchmark(
      nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
      const nighthawk::client::CommandLineOptions& command_line_options) override;
};

} // namespace Nighthawk
