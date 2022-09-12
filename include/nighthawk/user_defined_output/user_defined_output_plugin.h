#pragma once

#include <memory>

#include "absl/status/statusor.h"

#include "envoy/buffer/buffer.h"

#include "external/envoy/source/common/http/header_map_impl.h"

namespace Nighthawk {

/**
 * An interface for the UserDefinedOutputPlugin that receives responses and allows users to
 * attach their own custome output to each worker Result.
 *
 */
class UserDefinedOutputPlugin {
public:
  virtual ~UserDefinedOutputPlugin() = default;

  /**
   * Receives the headers from a single HTTP response, and allows the plugin to collect data based on
   * those headers.
   *
   * @return Status
   */
  virtual absl::Status handleResponseHeaders(const Envoy::Http::ResponseHeaderMapPtr&& headers) PURE;

  /**
   * Receives the data from a single HTTP response, and allows the plugin to collect data based on
   * that data.
   *
   * @return Status
   */
  virtual absl::Status handleResponseData(const Envoy::Buffer::Instance& response_data) PURE;

  /**
   * Get the output for this instance of the plugin, packed into an Any proto object.
   * 
   * @return absl::StatusOr<google::Protobuf::Any> 
   */
  virtual absl::StatusOr<Envoy::Protobuf::Message> getPerWorkerOutput() PURE;
};

using UserDefinedOutputPluginPtr = std::unique_ptr<UserDefinedOutputPlugin>;

}  // namespace Nighthawk