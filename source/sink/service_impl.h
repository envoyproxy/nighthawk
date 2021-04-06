#pragma once
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/sink/sink.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <memory>

#include "external/envoy/source/common/common/logger.h"

#include "nighthawk/sink/sink.h"

namespace Nighthawk {

/**
 * Transform a vector of ExecutionResponse messages into a single ExecutionResponse, by merging
 * associated outputs and error details.
 *
 * @param execution_id The execution-id that the responses are associated to.
 * @param responses The responses that should be merged into a single ExecutionResponse.
 * @return absl::StatusOr<nighthawk::client::ExecutionResponse> The merged response, or an error
 * status in case sanity checks failed.
 */
absl::StatusOr<nighthawk::client::ExecutionResponse>
mergeExecutionResponses(absl::string_view execution_id,
                        const std::vector<nighthawk::client::ExecutionResponse>& responses);

/**
 * Merge one output into another.
 *
 * @param source The source Output that should be merged into target.
 * @param target The target of the merge.
 * @return absl::Status Should be checked before proceeding to use target.
 */
absl::Status mergeOutput(const nighthawk::client::Output& source,
                         nighthawk::client::Output& target);

/**
 * Obtain a grpc::Status based on an absl::Status
 *
 * @param status To be translated.
 * @return grpc::Status the translated gRPC status.
 */
grpc::Status abslStatusToGrpcStatus(const absl::Status& status);

/**
 * Implements a real-world sink gRPC service.
 */
class SinkServiceImpl final : public nighthawk::NighthawkSink::Service,
                              public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  /**
   * Construct a new gRPC Sink service object
   *
   * @param sink Sink backend that will be used to load and store.
   */
  SinkServiceImpl(std::unique_ptr<Sink> sink);

  grpc::Status
  StoreExecutionResponseStream(grpc::ServerContext* context,
                               grpc::ServerReader<nighthawk::StoreExecutionRequest>* reader,
                               nighthawk::StoreExecutionResponse* response) override;

  grpc::Status SinkRequestStream(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<nighthawk::SinkResponse, nighthawk::SinkRequest>* stream) override;

private:
  std::unique_ptr<Sink> sink_;
};

} // namespace Nighthawk
