#include "nighthawk/adaptive_load/nighthawk_service_client.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {

class NighthawkServiceClientImpl : public NighthawkServiceClient {
public:
  absl::StatusOr<nighthawk::client::ExecutionResponse> PerformNighthawkBenchmark(
      nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
      const nighthawk::client::CommandLineOptions& command_line_options,
      const Envoy::Protobuf::Duration& duration) override;
};

} // namespace Nighthawk
