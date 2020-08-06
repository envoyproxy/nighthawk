#pragma once

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "absl/status/status.h"

namespace Nighthawk {

/**
 * Interface implemented by plugin config factories to perform proto-specific validations.
 */
class ConfigValidator {
public:
  virtual ~ConfigValidator() = default;
  /**
   * Check a config for plugin-specific errors.
   *
   * @param message The Any config proto taken from the TypedExtensionConfig that activated this
   * plugin, to be checked for validity in plugin-specific ways.
   *
   * @return Status OK for valid config, InvalidArgument with detailed error message otherwise.
   */
  virtual absl::Status ValidateConfig(__attribute__((unused))
                                      const Envoy::Protobuf::Message& message) {
    return absl::OkStatus();
  }
};

} // namespace Nighthawk
