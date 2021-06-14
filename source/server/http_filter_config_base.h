#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "external/envoy/source/common/common/statusor.h"

#include "api/server/response_options.pb.h"

#include "source/server/configuration.h"

#include "absl/status/status.h"

namespace Nighthawk {
namespace Server {

/**
 * Canonical representation of the effective filter configuration.
 * We use a shared pointer to avoid copying in the static configuration flow.
 */
using EffectiveFilterConfigurationPtr = std::shared_ptr<const nighthawk::server::ResponseOptions>;

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
  FilterConfigurationBase(const nighthawk::server::ResponseOptions& proto_config,
                          absl::string_view filter_name);

  /**
   * Copmute the effective configuration, based on considering the static configuration as well as
   * any configuration provided via request headers.
   *
   * @param request_headers Full set of request headers to be inspected for configuration.
   */
  void computeEffectiveConfiguration(const Envoy::Http::RequestHeaderMap& request_headers);

  /**
   * Send an error reply based on status of the effective configuration. For example, when dynamic
   * configuration delivered via request headers could not be parsed or was out of spec.
   *
   * @param decoder_callbacks Decoder used to generate the reply.
   * @return true iff an error reply was generated.
   */
  bool validateOrSendError(Envoy::Http::StreamDecoderFilterCallbacks& decoder_callbacks) const;

  /**
   * @brief Get the effective configuration. Depending on state ,this could be one of static
   * configuration, dynamic configuration, or an error status.
   *
   * @return const absl::StatusOr<EffectiveFilterConfigurationPtr> The effective configuration, or
   * an error status.
   */
  const absl::StatusOr<EffectiveFilterConfigurationPtr> getEffectiveConfiguration() const {
    return effective_config_;
  }

  /**
   * @return absl::string_view Name of the filter that constructed this instance.
   */
  absl::string_view filter_name() const { return filter_name_; }

private:
  const std::string filter_name_;
  const std::shared_ptr<nighthawk::server::ResponseOptions> server_config_;
  absl::StatusOr<EffectiveFilterConfigurationPtr> effective_config_;
};

} // namespace Server
} // namespace Nighthawk
