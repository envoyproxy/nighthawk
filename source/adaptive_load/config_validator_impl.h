#pragma once

#include "nighthawk/adaptive_load/config_validator.h"

namespace Nighthawk {

/**
 * A ConfigValidator that performs no checks and always returns OK.
 */
class NullConfigValidator : public virtual ConfigValidator {
public:
  absl::Status ValidateConfig(const Envoy::Protobuf::Message&) const override;
};

} // namespace Nighthawk
