#pragma once

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "absl/status/status.h"

namespace Nighthawk {

/**
 * Interface implemented by plugin config factories to perform proto-specific validations.
 *
 * The default ConfigValidator implementation performs no checks and returns OK.
 */
class ConfigValidator {
public:
  virtual ~ConfigValidator() = default;
  /**
   * Checks a config for plugin-specific errors.
   *
   * If the config proto contains any TypedExtensionConfig fields, ValidateConfig() should attempt
   * to call Load...Plugin() on each TypedExtensionConfig field value. See plugin_util.h. If all
   * plugin config factories follow this convention, the entire adaptive load session spec will be
   * recursively validated at load time.
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
