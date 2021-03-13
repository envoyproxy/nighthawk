#pragma once
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wunused-parameter"
#endif
#include "api/sink/sink.grpc.pb.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <future>
#include <map>
#include <memory>

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/thread.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/exe/process_wide.h"

#include "nighthawk/client/process.h"
#include "nighthawk/sink/sink.h"

namespace Nighthawk {

class SinkServiceImpl final : public nighthawk::NighthawkSink::Service,
                              public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {

public:
  SinkServiceImpl(std::unique_ptr<Sink>&& sink);
  ::grpc::Status
  StoreExecutionResponseStream(::grpc::ServerContext* context,
                               ::grpc::ServerReader<::nighthawk::StoreExecutionRequest>* reader,
                               ::nighthawk::StoreExecutionResponse* response) override;

  ::grpc::Status
  SinkRequestStream(::grpc::ServerContext* context,
                    ::grpc::ServerReaderWriter<::nighthawk::SinkResponse, ::nighthawk::SinkRequest>*
                        stream) override;

private:
  const std::map<const std::string, const StatisticPtr>
  readAppendices(const std::vector<::nighthawk::client::ExecutionResponse>& responses) const;
  absl::StatusOr<::nighthawk::SinkResponse> aggregateSinkResponses(
      absl::string_view requested_execution_id,
      const std::vector<::nighthawk::client::ExecutionResponse>& responses) const;
  const absl::Status mergeIntoAggregatedOutput(const ::nighthawk::client::Output& input_to_merge,
                                               ::nighthawk::client::Output& merge_target) const;
  // TODO(oschaaf): ref?
  std::unique_ptr<Sink> sink_;
};

} // namespace Nighthawk
