#pragma once

#include <memory>

#include "envoy/buffer/buffer.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy_api/envoy/config/core/v3/extension.pb.h"

#include "api/client/output.pb.h"

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
 * concurrently. In addition, handleResponseData and handleResponseHeaders may be called in any
 * order or possibly concurrently. GetPerWorkerOutput is guaranteed to be called after
 * handleResponseData and handleResponseHeaders have been called for every response in its worker
 * thread.
 *
 * handleResponseData and handleResponseHeaders are only called on valid http responses for which
 * the request actually was sent out. For more information, see
 * https://www.envoyproxy.io/docs/envoy/latest/intro/life_of_a_request#http-filter-chain-processing.
 * They are each called by the corresponding decode api.
 *
 * Note that GetPerWorkerOutput will be called regardless of whether or not
 * handleResponseHeaders/handleResponseData were ever successfully called.
 */
class UserDefinedOutputPlugin {
public:
  virtual ~UserDefinedOutputPlugin() = default;

  /**
   * Receives the headers from a single HTTP response, and allows the plugin to collect data based
   * on those headers.
   *
   * Plugins should return statuses for invalid data or when they fail to process the data. Any
   * non-ok status will be logged and increment a counter
   * (benchmark.user_defined_plugin_handle_headers_failure) that will be added to the worker Result.
   * Callers can also provide a failure predicate for this counter that will abort the request
   * after n plugin failures.
   *
   * Must be thread safe.
   *
   * @param headers
   */
  virtual absl::Status handleResponseHeaders(const Envoy::Http::ResponseHeaderMap& headers) PURE;

  /**
   * Receives a single response body, and allows the plugin to collect data based on
   * that response body.
   *
   * Plugins should return statuses for invalid data or when they fail to process the data. Any
   * non-ok status will be logged and increment a counter
   * (benchmark.user_defined_plugin_handle_data_failure) that will be added to the worker Result.
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
   * non-ok status will be logged and included as a UserDefinedOutput with an error_message instead
   * of a typed_output. Standard nighthawk processing will be unaffected.
   *
   * @return output Any-packed per_worker output to add to the worker's Result.
   */
  virtual absl::StatusOr<Envoy::ProtobufWkt::Any> getPerWorkerOutput() const PURE;
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
  virtual absl::StatusOr<UserDefinedOutputPluginPtr>
  createUserDefinedOutputPlugin(const Envoy::ProtobufWkt::Any& typed_config,
                                const WorkerMetadata& worker_metadata) PURE;

  /**
   * Aggregates the outputs from every worker's UserDefinedOutputPlugin instance into a global
   * output, representing the cumulative data across all of the plugins combined.
   *
   * If a plugin returned an error when generating its per-worker output, it will still be included
   * in per_worker_outputs as a UserDefinedOutput with an error message. It is up to the plugin
   * author the correct thing to do on aggregation if one or more of the per worker outputs
   * contains errors.
   *
   * This method should return statuses for invalid data or when they fail to process the data. Any
   * non-ok status will be logged and included as a UserDefinedOutput with an error_message instead
   * of a typed_output. Standard nighthawk processing will be unaffected.
   *
   * Pseudocode Example:
   *     AggregateGlobalOutput(
   *      {int_value: 1, array_value: ["a"]}, {int_value: 2, array_value: ["b","c"]}
   *     )
   *   Might return:
   *     {int_value: 3, array_value: ["a","b","c"]}
   *
   * @param per_worker_outputs List of the outputs that every per-worker instance of the User
   *    Defined Output Plugin created, including errors in generating that output.

   * @return global_output Any-packed aggregated output to add to the global Result.
   */
  virtual absl::StatusOr<Envoy::ProtobufWkt::Any> AggregateGlobalOutput(
      absl::Span<const nighthawk::client::UserDefinedOutput> per_worker_outputs) PURE;
};

using UserDefinedOutputConfigFactoryPair =
    std::pair<envoy::config::core::v3::TypedExtensionConfig, UserDefinedOutputPluginFactory*>;
using UserDefinedOutputNamePluginPair = std::pair<std::string, UserDefinedOutputPluginPtr>;

} // namespace Nighthawk
