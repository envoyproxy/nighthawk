#include "source/distributor/distributed_process_impl.h"

#include <grpc++/grpc++.h>

#include <memory>

#include "nighthawk/client/output_collector.h"

#include "external/envoy/source/common/common/random_generator.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "source/client/options_impl.h"
#include "source/common/uri_impl.h"

namespace Nighthawk {

DistributedProcessImpl::DistributedProcessImpl(
    const Client::Options& options,
    nighthawk::NighthawkDistributor::StubInterface& distributor_stub,
    nighthawk::NighthawkSink::StubInterface& sink_stub)
    : options_(options), distributor_client_(std::make_unique<NighthawkDistributorClientImpl>()),
      distributor_stub_(distributor_stub),
      sink_client_(std::make_unique<NighthawkSinkClientImpl>()), sink_stub_(sink_stub) {}

absl::StatusOr<const nighthawk::DistributedResponse> DistributedProcessImpl::sendDistributedRequest(
    const ::nighthawk::DistributedRequest& request) const {
  const absl::StatusOr<const nighthawk::DistributedResponse> result =
      distributor_client_->DistributedRequest(distributor_stub_, request);
  if (!result.ok()) {
    ENVOY_LOG(error, "Distributed request failure: {}", result.status().message());
  } else {
    ENVOY_LOG(trace, "Distributed response: {}", result.value().DebugString());
  }
  return result;
}

absl::StatusOr<const nighthawk::SinkResponse>
DistributedProcessImpl::sendSinkRequest(const ::nighthawk::SinkRequest& request) const {
  const absl::StatusOr<const nighthawk::SinkResponse> result =
      sink_client_->SinkRequestStream(sink_stub_, request);
  if (!result.ok()) {
    ENVOY_LOG(error, "Sink request failure: {}", result.status().message());
  } else {
    ENVOY_LOG(trace, "Sink response: {}", result.value().DebugString());
  }
  return result;
}

bool DistributedProcessImpl::run(Client::OutputCollector& collector) {
  Client::CommandLineOptionsPtr options = options_.toCommandLineOptions();
  if (!options_.sink().has_value()) {
    // TODO(XXX): without a sink, the request above could yield a full execution response,
    // Alternatively, we error out completely and reject early instead today.
    ENVOY_LOG(error, "Distributed request MUST have a sink configured today.");
    return false;
  }
  if (!options->has_execution_id()) {
    Envoy::Random::RandomGeneratorImpl random_generator;
    const std::string uuid = random_generator.uuid();
    options->mutable_execution_id()->set_value(uuid);
  }
  const std::string execution_id = options->execution_id().value();
  ENVOY_LOG(info, "Using execution id '{}'", execution_id);
  ::nighthawk::DistributedRequest request;
  *(request.mutable_execution_request()->mutable_start_request()->mutable_options()) = *options;
  request.mutable_execution_request()
      ->mutable_start_request()
      ->mutable_options()
      ->mutable_execution_id()
      ->set_value(execution_id);
  if (options_.services().has_value()) {
    *(request.mutable_services()) = options_.services().value().addresses();
  }
  const absl::StatusOr<const nighthawk::DistributedResponse> distributed_initiation_response =
      sendDistributedRequest(request);
  if (!distributed_initiation_response.ok()) {
    return false;
  }
  // If we could initiate the distributed load test, then we can now query the sink to obtain
  // results with the execution_id we obtained through that.
  // TODO(XXX): set a sensible timeout, or do so on the other side.
  nighthawk::SinkRequest sink_request;
  // distributed_sink_request.add_services()->MergeFrom(options_.sink().value().address());
  sink_request.set_execution_id(execution_id);

  absl::StatusOr<const nighthawk::SinkResponse> sink_response = sendSinkRequest(sink_request);
  if (sink_response.ok()) {
    const ::nighthawk::client::Output& output = sink_response.value().execution_response().output();
    collector.setOutput(output);
    bool has_failed_termination = false;
    for (const ::nighthawk::client::Result& result : output.results()) {
      for (const ::nighthawk::client::Counter& counter : result.counters()) {
        if (counter.name() == "sequencer.failed_terminations") {
          has_failed_termination = true;
          break;
        }
      }
    }
    return !has_failed_termination;
  }
  return false;
}

bool DistributedProcessImpl::requestExecutionCancellation() {
  ENVOY_LOG(error, "Distributed process cancellation not supported yet");
  // TODO(#380): Send a cancel request to the gRPC service.
  return false;
}

} // namespace Nighthawk
