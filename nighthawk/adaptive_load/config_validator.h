#pragma once

#include "external/envoy/envoy/common/pure.h"
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
   * Checks a config for plugin-specific errors.
   *
   * If the config proto contains any TypedExtensionConfig fields, ValidateConfig() should attempt
   * to call Load...Plugin() on each TypedExtensionConfig field value. See plugin_util.h. If all
   * plugin config factories follow this convention, the entire adaptive load session spec will be
   * recursively validated at load time.
   *
   * Any validation errors should be encoded in the absl::Status return object; do not throw an
   * exception.
   *
   * In the absence of fields to check, just return absl::OkStatus() immediately.
   *
   * This method is not responsible for checking the type of |message|. If |message| is the wrong
   * type, this will be detected elsewhere during plugin creation and handled cleanly.
   *
   * To inspect the content of |message|, directly attempt to unpack it to the plugin-specific proto
   * type, without specially checking for errors. If it is the wrong type, the unpacking will throw
   * EnvoyException, and the caller will handle it.
   *
   * @param message The Any config proto taken from the TypedExtensionConfig that activated this
   * plugin, to be checked for validity in plugin-specific ways.
   *
   * @return Status OK for valid config, InvalidArgument with detailed error message otherwise.
   *
   * @throw EnvoyException Only if unpacking |message| fails; otherwise return absl::Status.
   */
  virtual absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const PURE;
};

} // namespace Nighthawk
