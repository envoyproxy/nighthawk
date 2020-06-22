#include "client/remote_process_impl.h"

#include <grpc++/grpc++.h>

#include <memory>

#include "nighthawk/client/output_collector.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "common/uri_impl.h"

#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

RemoteProcessImpl::RemoteProcessImpl(const Options& options,
                                     nighthawk::client::NighthawkService::Stub& stub)
    : options_(options), stub_(stub) {}

bool RemoteProcessImpl::run(OutputCollector& collector) {
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::ExecutionResponse response;
  grpc::ClientContext context;
  auto execution_stream = stub_.ExecutionStream(&context);

  *request.mutable_start_request()->mutable_options() = *options_.toCommandLineOptions();
  // We don't forward the option that requests remote execution. Today,
  // nighthawk_service will ignore the option, but if someone ever changes that this
  // is probably desireable.
  request.mutable_start_request()->mutable_options()->mutable_nighthawk_service()->Clear();

  if (execution_stream->Write(request, {}) && execution_stream->Read(&response)) {
    if (response.has_output()) {
      collector.setOutput(response.output());
    } else {
      ENVOY_LOG(error, "Remote execution failed");
    }
    if (response.has_error_detail()) {
      ENVOY_LOG(error, "have error detail: {}", response.error_detail().DebugString());
    }
    if (!execution_stream->WritesDone()) {
      ENVOY_LOG(warn, "writeDone() failed");
    } else {
      auto status = execution_stream->Finish();
      return status.ok();
    }
  } else {
    ENVOY_LOG(error, "Failure while communicating with the remote service");
  }

  return false;
}

bool RemoteProcessImpl::requestExecutionCancellation() {
  ENVOY_LOG(error, "Remote process cancellation not supported yet");
  // TODO(#380): Send a cancel request to the gRPC service.
  return false;
}

} // namespace Client
} // namespace Nighthawk
