#include "source/adaptive_load/config_validator_impl.h"

namespace Nighthawk {

absl::Status NullConfigValidator::ValidateConfig(const Envoy::Protobuf::Message&) const {
  return absl::OkStatus();
}

} // namespace Nighthawk
