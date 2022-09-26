#pragma once

#include <memory>

#include "envoy/buffer/buffer.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/http/header_map_impl.h"

namespace Nighthawk {

// Information about a Nighthawk worker thread. May expand to contain more fields over time as
// desired.
struct WorkerMetadata {
  // Identifies which worker instantiated the plugin instance.
  int worker_number;
};

/**
 * An interface for the UserDefinedOutputPlugin that receives responses and allows users to
 * attach their own custom output to each worker Result.
 *
 * All UserDefinedOutputPlugins must be thread safe, as it may receive multiple responses
 * simultaneously. In addition, handleResponseData and handleResponseHeaders may be called in any
 * order or possibly concurrently. GetPerWorkerOutput is guaranteed to be called after
 * handleResponseData and handleResponseHeaders have been called for every relevant response.
 *
 * TODO(dubious90): Throughout file, update comments to contain counter names and other related
 * specifics.
 *
 * TODO(dubious90): Comment on behavior of "relevant responses". e.g. If this plugin still gets
 * called on pool_overflows or other edge cases
 */
class UserDefinedOutputPlugin {
public:
  virtual ~UserDefinedOutputPlugin() = default;

  /**
   * Receives the headers from a single HTTP response, and allows the plugin to collect data based
   * on those headers.
   *
   * Plugins should return statuses for invalid data or when they fail to process the data. Any
   * non-ok status will be logged and increment a counter that will be added to the worker Result.
   * Callers can also provide a failure predicate for this counter that will abort the request
   * after n plugin failures.
   *
   * Must be thread safe.
   *
   * @param headers
   */
  virtual absl::Status
  handleResponseHeaders(const Envoy::Http::ResponseHeaderMapPtr&& headers) PURE;

  /**
   * Receives a single response body, and allows the plugin to collect data based on
   * that response body.
   *
   * Plugins should return statuses for invalid data or when they fail to process the data. Any
   * non-ok status will be logged and increment a counter that will be added to the worker Result.
   * Callers can also provide a failure predicate for this counter that will abort the request
   * after n plugin failures.
   *
   * Must be thread safe.
   *
   * @param response_data
   */
  virtual absl::Status handleResponseData(const Envoy::Buffer::Instance& response_data) PURE;

  /**
   * Get the output for this instance of the plugin, packed into an Any proto object.
   *
   * Nighthawk ensures that this is called after responses are returned. However, if a plugin's
   * handleResponseHeaders or handleResponseData do any asynchronous work, this method should
   * ensure that handleResponseData and handleResponseHeaders are completed before this function
   * processes.
   *
   * Plugins should return statuses for invalid data or when they fail to process the data. Any
   * non-ok status will be logged and increment a counter that will be added to the worker Result.
   * Callers can also provide a failure predicate for this counter that will abort the request
   * after n plugin failures.
   *
   * @return output Any-packed per_worker output to add to the worker's Result.
   */
  virtual absl::StatusOr<Envoy::ProtobufWkt::Any> getPerWorkerOutput() PURE;
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
  virtual UserDefinedOutputPluginPtr
  createUserDefinedOutputPlugin(const Envoy::Protobuf::Message& typed_config,
                                const WorkerMetadata& worker_metadata) PURE;

  /**
   * Aggregates the outputs from every worker's UserDefinedOutputPlugin instance into a global
   * output, representing the cumulative data across all of the plugins combined.
   *
   * The protobuf type of the inputs and output must all be the same type.
   *
   * This method should return statuses for invalid data or when they fail to process the data. Any
   * non-ok status will be logged and increment a counter that will be added to the worker Result.
   * Callers can also provide a failure predicate for this counter that will abort the request
   * after n plugin failures.
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

   * @return global_output Any-packed aggregated output to add to the global Result.
   */
  virtual absl::StatusOr<Envoy::ProtobufWkt::Any>
  AggregateGlobalOutput(absl::Span<Envoy::ProtobufWkt::Any> per_worker_outputs) PURE;
};
} // namespace Nighthawk
