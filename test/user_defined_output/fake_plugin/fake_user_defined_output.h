#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/user_defined_output/user_defined_output_plugin.h"

#include "external/envoy/source/common/common/statusor.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"

#include "test/user_defined_output/fake_plugin/fake_user_defined_output.pb.h"

namespace Nighthawk {

/**
 * UserDefinedOutputPlugin for testing: Counts the number of times each api method is called,
 * and also allows a failure setting after a certain number of calls for each method.
 *
 * This plugin should be used in tests to prove that plugins receive the correct calls and can
 * handle failures appropriately.
 *
 * This class is not thread-safe.
 * TODO(dubious90): This plugin must be thread safe.
 */
class FakeUserDefinedOutputPlugin : public UserDefinedOutputPlugin {
public:
  /**
   * Initializes the User Defined Output Plugin.
   *
   * @param config FakeUserDefinedOutputConfig proto for setting the fixed RPS value.
   * @param worker_metadata Information from the calling worker.
   */
  FakeUserDefinedOutputPlugin(nighthawk::FakeUserDefinedOutputConfig config,
                              WorkerMetadata worker_metadata);

  /**
   * Receives the headers from a single HTTP response. Increments headers_called_.
   */
  absl::Status handleResponseHeaders(const Envoy::Http::ResponseHeaderMapPtr&& headers) override;

  /**
   * Receives the data from a single HTTP response. Increments data_called_.
   */
  absl::Status handleResponseData(const Envoy::Buffer::Instance& response_data) override;

  /**
   * Get the output for this instance of the plugin, packing it into output.
   */
  absl::StatusOr<google::protobuf::Any> getPerWorkerOutput() override;

private:
  int data_called_ = 0;
  int headers_called_ = 0;
  const nighthawk::FakeUserDefinedOutputConfig config_;
  const WorkerMetadata worker_metadata_;
};

/**
 * Factory that creates a FakeUserDefinedOutputPlugin plugin from a FakeUserDefinedOutputConfig
 * proto. Registered as an Envoy plugin.
 */
class FakeUserDefinedOutputPluginFactory : public UserDefinedOutputPluginFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  UserDefinedOutputPluginPtr
  createUserDefinedOutputPlugin(const Envoy::Protobuf::Message& config_any,
                                const WorkerMetadata& worker_metadata) override;

  absl::StatusOr<google::protobuf::Any>
  AggregateGlobalOutput(absl::Span<const google::protobuf::Any> per_worker_outputs) override;
};

// This factory is activated through LoadStepControllerPlugin in plugin_util.h.
DECLARE_FACTORY(FakeUserDefinedOutputPluginFactory);

} // namespace Nighthawk
