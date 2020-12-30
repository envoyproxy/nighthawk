#include "client/remote_process_impl.h"

#include <grpc++/grpc++.h>

#include <memory>

#include "nighthawk/client/output_collector.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "common/nighthawk_service_client_impl.h"
#include "common/uri_impl.h"

#include "client/options_impl.h"

#include "absl/status/status.h"

namespace Nighthawk {
namespace Client {

RemoteProcessImpl::RemoteProcessImpl(const Options& options,
                                     nighthawk::client::NighthawkService::Stub& stub)
    : options_(options), service_client_(std::make_unique<NighthawkServiceClientImpl>()),
      stub_(stub) {}

absl::Status RemoteProcessImpl::run(OutputCollector& collector) {
  Nighthawk::Client::CommandLineOptionsPtr options = options_.toCommandLineOptions();
  // We don't forward the option that requests remote execution. Today,
  // nighthawk_service will ignore the option, but if someone ever changes that this
  // is probably desireable.
  options->mutable_nighthawk_service()->Clear();

  const absl::StatusOr<const nighthawk::client::ExecutionResponse> result =
      service_client_->PerformNighthawkBenchmark(&stub_, *options);
  if (result.ok()) {
    collector.setOutput(result.value().output());
    return absl::OkStatus();
  }
  ENVOY_LOG(error, "Remote execution failure: {}", result.status().message());
  return absl::Status(static_cast<absl::StatusCode>(result.status().code()),
                      absl::StrCat("Remote execution failure: ", result.status().message()));
}

bool RemoteProcessImpl::requestExecutionCancellation() {
  ENVOY_LOG(error, "Remote process cancellation not supported yet");
  // TODO(#380): Send a cancel request to the gRPC service.
  return false;
}

} // namespace Client
} // namespace Nighthawk
