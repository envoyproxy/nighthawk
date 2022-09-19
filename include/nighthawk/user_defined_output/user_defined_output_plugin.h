#pragma once

#include <memory>

#include "envoy/buffer/buffer.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/http/header_map_impl.h"


namespace Nighthawk {

// Information about a Nighthawk worker thread. May expand to contain more fields over time as
// desired.
struct WorkerMetadata {
  std::string name;
};

/**
 * An interface for the UserDefinedOutputPlugin that receives responses and allows users to
 * attach their own custome output to each worker Result.
 *
 */
class UserDefinedOutputPlugin {
public:
  virtual ~UserDefinedOutputPlugin() = default;

  /**
   * Receives the headers from a single HTTP response, and allows the plugin to collect data based
   * on those headers.
   * 
   * @param headers
   */
  virtual absl::Status handleResponseHeaders(
    const Envoy::Http::ResponseHeaderMapPtr&& headers) PURE;

  /**
   * Receives the data from a single HTTP response, and allows the plugin to collect data based on
   * that data.
   * 
   * @param response_data
   */
  virtual absl::Status handleResponseData(const Envoy::Buffer::Instance& response_data) PURE;

  /**
   * Get the output for this instance of the plugin, packed into an Any proto object.
   * 
   * @param output Any object to pack the per_worker output into to add to the global Result.
   */
  virtual absl::Status getPerWorkerOutput(google::protobuf::Any& output_any) PURE;
};

using UserDefinedOutputPluginPtr = std::unique_ptr<UserDefinedOutputPlugin>;

// A factory that must be implemented for each UserDefinedOutput plugin. It instantiates the
// specific UserDefinedPlugin class after unpacking the plugin-specific config proto.
class UserDefinedOutputPluginFactory : public Envoy::Config::TypedFactory {
public:
  ~UserDefinedOutputPluginFactory() override = default;

  std::string category() const override { return "nighthawk.user_defined_output_plugin"; }

  /**
   * Instantiates the specific UserDefinedOutputPlugin class. Casts |message| to Any, unpacks it
   * to the plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
   *
   * @param typed_config Any typed_config proto taken from the TypedExtensionConfig.
   * @param worker_metadata Details about the worker that is creating this plugin, can be used
   * optionally as needed.
   *
   * @return UserDefinedOutputPluginPtr Pointer to the new instance of UserDefinedOutputPlugin.
   *
   * @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
   * plugin.   
  */
  virtual UserDefinedOutputPluginPtr createUserDefinedOutputPlugin(
    const Envoy::Protobuf::Message& typed_config, const WorkerMetadata& worker_metadata) PURE;

  /**
   * Aggregates the outputs from every worker's UserDefinedOutputPlugin instance into a global
   * output, representing the cumulative data across all of the plugins combined.
   * 
   * The protobuf type of the inputs and output must all be the same type.
   * 
   * Pseudocode Example:
   *     AggregateGlobalOutput(
   *      {int_value: 1, array_value: ["a"]}, {int_value: 2, array_value: ["b","c"]}
   *     )
   *   Might return:
   *     {int_value: 3, array_value: ["a","b","c"]}
   * 
   * @param per_worker_outputs List of the outputs that every per-worker instance of the User
   *    Defined Output Plugin created.
   * @param global_output Any object to pack the aggregated output into to add to the global
   *    Result.
   * @return absl::Status
   */
  virtual absl::Status AggregateGlobalOutput(
    absl::Span<const google::protobuf::Any> per_worker_outputs,
    google::protobuf::Any& global_output_any) PURE;
};
}  // namespace Nighthawk