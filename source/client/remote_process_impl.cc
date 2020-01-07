#include "client/remote_process_impl.h"

#include <grpc++/grpc++.h>

#include <memory>

#include "nighthawk/client/output_collector.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"

#include "common/uri_impl.h"

#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

RemoteProcessImpl::RemoteProcessImpl(const Options& options) : options_(options) {}

bool RemoteProcessImpl::run(OutputCollector& collector) {
  UriPtr uri;

  try {
    uri = std::make_unique<UriImpl>(options_.nighthawkService());
  } catch (const UriException&) {
    ENVOY_LOG(error, "Bad service uri: {}", options_.nighthawkService());
    return false;
  }

  auto channel = grpc::CreateChannel(fmt::format("{}:{}", uri->hostWithoutPort(), uri->port()),
                                     grpc::InsecureChannelCredentials());
  auto stub = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel);
  grpc::ClientContext context;
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::ExecutionResponse response;
  auto r = stub->ExecutionStream(&context);

  *request.mutable_start_request()->mutable_options() = *options_.toCommandLineOptions();
  // We don't forward the option that requests remote execution. Today,
  // nighthawk_service will ignore the option, but if someone ever changes that this
  // is probably desireable.
  request.mutable_start_request()->mutable_options()->mutable_nighthawk_service()->Clear();

  if (r->Write(request, {}) && r->Read(&response)) {
    if (response.has_output()) {
      collector.setOutput(response.output());
    } else {
      ENVOY_LOG(error, "Remote execution failed");
    }
    if (response.has_error_detail()) {
      ENVOY_LOG(error, "have error detail: {}", response.error_detail().DebugString());
    }
    if (!r->WritesDone()) {
      ENVOY_LOG(warn, "writeDone() failed");
    } else {
      auto status = r->Finish();
      return status.ok();
    }
  } else {
    ENVOY_LOG(error, "Failure while communicating with the remote service");
  }

  return false;
}

} // namespace Client
} // namespace Nighthawk
