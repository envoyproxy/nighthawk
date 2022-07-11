#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.h"

#include "source/server/configuration.h"

#include "absl/status/status.h"

namespace Nighthawk {
namespace Server {

/**
 * Provides functionality for parsing and merging request-header based configuration, as well as
 * generating a common error response accross all extensions.
 */
class FilterConfigurationBase {
public:
  /**
   * @brief Construct a new Filter Configuration Base object
   *
   * @param proto_config the static disk-based response options configuration
   * @param filter_name name of the extension that is consuming this. Used during error response
   * generation.
   */
  FilterConfigurationBase(absl::string_view filter_name);

  /**
   * Send an error reply based on status of the effective configuration. For example, when dynamic
   * configuration delivered via request headers could not be parsed or was out of spec.
   *
   * @param effective_config Effective filter configuration.
   * @param decoder_callbacks Decoder used to generate the reply.
   * @return true iff an error reply was generated.
   */
  bool validateOrSendError(const absl::Status& effective_config,
                           Envoy::Http::StreamDecoderFilterCallbacks& decoder_callbacks) const;

  /**
   * @return absl::string_view Name of the filter that constructed this instance.
   */
  absl::string_view filter_name() const { return filter_name_; }

private:
  const std::string filter_name_;
};

} // namespace Server
} // namespace Nighthawk
