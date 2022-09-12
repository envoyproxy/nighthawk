#pragma once

#include "absl/status/statusor.h"

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "nighthawk/user_defined_output/user_defined_output_plugin.h"

namespace Nighthawk {

// Information about a Nighthawk worker thread. May expand to contain more fields over time as desired.
struct WorkerMetadata {
  std::string name;
};

// A factory that must be implemented for each UserDefinedOutput plugin. It instantiates the
// specific UserDefinedPlugin class after unpacking the plugin-specific config proto.
class UserDefinedOutputPluginFactory : public Envoy::Config::TypedFactory {
public:
  ~UserDefinedOutputPluginFactory() override = default;

  std::string category() const override { return "nighthawk.user_defined_output_plugin"; }

  /**
   * Instantiates the specific UserDefinedOutputPlugin class. Casts |message| to Any, unpacks it to the
   * plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
   *
   * @param typed_config Any typed_config proto taken from the TypedExtensionConfig.
   * @param worker_metadata Details about the worker that is creating this plugin, can be used optionally as needed.
   *
   * @return UserDefinedOutputPluginPtr Pointer to the new instance of UserDefinedOutputPlugin.
   *
   * @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
   * plugin.   
  */
  virtual UserDefinedOutputPluginPtr createUserDefinedOutputPlugin(const Envoy::Protobuf::Message& typed_config, WorkerMetadata worker_metadata) PURE;

  /**
   * Aggregates the outputs from every worker's UserDefinedOutputPlugin instance into a global output, representing the cumulative data
   * across all of the plugins combined.
   * 
   * The protobuf type of the inputs and output must all be the same type.
   * 
   * Pseudocode Example:
   *     AggregateGlobalOutput({int_value: 1, array_value: ["a"]}, {int_value: 2, array_value: ["b","c"]})
   *   Might return:
   *     {int_value: 3, array_value: ["a","b","c"]}
   * 
   * @param per_worker_outputs List of the outputs that every per-worker instance of the User Defined Output Plugin created.
   * @return absl::StatusOr<Envoy::Protobuf::Message> an aggregated output to add to the global Result.
   */
  virtual absl::StatusOr<Envoy::Protobuf::Message> AggregateGlobalOutput(absl::Span<const Envoy::Protobuf::Message> per_worker_outputs) PURE;
};

} // namespace Nighthawk
