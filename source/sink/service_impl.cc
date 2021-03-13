#include "sink/service_impl.h"

#include <grpc++/grpc++.h>

#include "envoy/config/core/v3/base.pb.h"

#include "client/client.h"
#include "client/options_impl.h"
#include "client/output_collector_impl.h"

#include "sink/nighthawk_sink_client_impl.h"
#include "sink/sink_impl.h"

namespace Nighthawk {

using ::Envoy::Protobuf::util::MessageDifferencer;

SinkServiceImpl::SinkServiceImpl(std::unique_ptr<Sink>&& sink) : sink_(std::move(sink)) {}

::grpc::Status SinkServiceImpl::StoreExecutionResponseStream(
    ::grpc::ServerContext*,
    ::grpc::ServerReader<::nighthawk::StoreExecutionRequest>* request_reader,
    ::nighthawk::StoreExecutionResponse*) {
  nighthawk::StoreExecutionRequest request;
  while (request_reader->Read(&request)) {
    ENVOY_LOG(info, "StoreExecutionResponseStream request {}", request.DebugString());
    const ::nighthawk::client::ExecutionResponse& response_to_store = request.execution_response();
    const auto status = sink_->StoreExecutionResultPiece(response_to_store);
    if (!status.ok()) {
      ENVOY_LOG(error, "StoreExecutionResponseStream failure: {}", status.ToString());
      return ::grpc::Status(grpc::StatusCode::INTERNAL, status.ToString());
    }
  }
  return ::grpc::Status::OK;
};

const absl::Status
SinkServiceImpl::mergeIntoAggregatedOutput(const ::nighthawk::client::Output& input_to_merge,
                                           ::nighthawk::client::Output& merge_target) const {
  if (!merge_target.has_options()) {
    // If no options are set, that means this is the first part of the merge.
    // Set some properties that shouldbe equal amongst all Output instances.
    *(merge_target.mutable_options()) = input_to_merge.options();
    *(merge_target.mutable_timestamp()) = input_to_merge.timestamp();
    *(merge_target.mutable_version()) = input_to_merge.version();
  } else {
    // Options used should not diverge for a executions under a single execution id.
    // Versions probably shouldn't either. We sanity check these things here, and
    // report on error when we detect any mismatch.
    if (!MessageDifferencer::Equivalent(input_to_merge.options(), merge_target.options())) {
      // TODO(XXX): have to exclude certain options here, like output-format, which are
      // not relevant to actual execution.
      return absl::Status(
          absl::StatusCode::kInternal,
          fmt::format("Options divergence detected across x-service results: {} vs {}.",
                      merge_target.options().DebugString(),
                      input_to_merge.options().DebugString()));
    }
    if (!MessageDifferencer::Equivalent(input_to_merge.version(), merge_target.version())) {
      return absl::Status(
          absl::StatusCode::kInternal,
          fmt::format("Version divergence detected across x-service results: {} vs {}.",
                      merge_target.version().DebugString(),
                      input_to_merge.version().DebugString()));
    }
    // TODO(XXX): Consider validating that the associated timestamps are are close enough.
  }
  // Append all input results into our own results.
  // TODO(XXX): make sure we can distinct results from multiple sources.
  for (const auto& result : input_to_merge.results()) {
    merge_target.add_results()->MergeFrom(result);
  }
  // The final step is the more complicated one: aggregate the top level result derived
  // from the per-service execution results.
  return absl::OkStatus();
}

const std::map<const std::string, const StatisticPtr> SinkServiceImpl::readAppendices(
    const std::vector<::nighthawk::client::ExecutionResponse>& responses) const {
  for (const auto& response : responses) {
    if (response.has_appendix()) {
      ENVOY_LOG(error, "fix appending handling");
    }
  }
  return std::map<const std::string, const StatisticPtr>();
}

absl::StatusOr<::nighthawk::SinkResponse> SinkServiceImpl::aggregateSinkResponses(
    absl::string_view requested_execution_id,
    const std::vector<::nighthawk::client::ExecutionResponse>& responses) const {
  if (responses.size() == 0) {
    return absl::Status(
        absl::StatusCode::kInternal,
        "sink->LoadExecutionResult yielded an empty vector, and broke its promise.");
  }

  ::nighthawk::SinkResponse response;
  ::nighthawk::client::ExecutionResponse* aggregated_response =
      response.mutable_execution_response();
  ::nighthawk::client::Output aggregated_output;
  aggregated_response->mutable_execution_id()->assign(requested_execution_id);
  for (const ::nighthawk::client::ExecutionResponse& execution_response : responses) {
    if (execution_response.execution_id() != requested_execution_id) {
      return absl::Status(absl::StatusCode::kInternal,
                          "sink->LoadExecutionResult yielded a result with a bad execution id!");
    }
    // If any error exists, set an error code and message & append the details of each such
    // occurrence.
    if (execution_response.has_error_detail()) {
      ::google::rpc::Status* error_detail = aggregated_response->mutable_error_detail();
      error_detail->set_code(-1);
      error_detail->set_message("One or more remote execution(s) terminated with a failure.");
      error_detail->add_details()->PackFrom(execution_response.error_detail());
    }

    if (execution_response.has_appendix()) {
      ENVOY_LOG(error, "TODO: Appendix handling!");
    } else if (execution_response.has_output()) {
      absl::Status merge_status =
          mergeIntoAggregatedOutput(execution_response.output(), aggregated_output);
      if (!merge_status.ok()) {
        return merge_status;
      }
    } else {
      return absl::Status(
          absl::StatusCode::kInternal,
          "sink->LoadExecutionResult yielded a result with neither an appendix or output set!");
    }
  }

  *(aggregated_response->mutable_output()) = aggregated_output;
  return response;
}

::grpc::Status SinkServiceImpl::SinkRequestStream(
    ::grpc::ServerContext*,
    ::grpc::ServerReaderWriter<::nighthawk::SinkResponse, ::nighthawk::SinkRequest>* stream) {
  ::nighthawk::SinkRequest request;
  absl::Status status = absl::OkStatus();
  while (stream->Read(&request)) {
    ENVOY_LOG(trace, "Inbound SinkRequest {}", request.DebugString());
    absl::StatusOr<std::vector<::nighthawk::client::ExecutionResponse>>
        status_or_execution_responses;
    do {
      status_or_execution_responses = sink_->LoadExecutionResult(request.execution_id());
      sleep(1);
    } while (!status_or_execution_responses.ok());
    status.Update(status_or_execution_responses.status());
    if (status.ok()) {
      const std::vector<::nighthawk::client::ExecutionResponse>& responses =
          status_or_execution_responses.value();
      const std::map<const std::string, const StatisticPtr> stat_by_appendix_id =
          readAppendices(responses);
      absl::StatusOr<::nighthawk::SinkResponse> response =
          aggregateSinkResponses(request.execution_id(), responses);
      status.Update(response.status());
      if (status.ok() && !stream->Write(response.value())) {
        status.Update(
            absl::Status(absl::StatusCode::kInternal, "Failure writing response to stream."));
      }
    }
    if (!status.ok()) {
      ENVOY_LOG(error, "Failure while handling SinkRequest: {} -> {}", request.DebugString(),
                status.ToString());
      break;
    }
  }
  ENVOY_LOG(trace, "Finishing stream");
  return status.ok() ? grpc::Status::OK
                     : grpc::Status(grpc::StatusCode::INTERNAL, status.ToString());
}

} // namespace Nighthawk
