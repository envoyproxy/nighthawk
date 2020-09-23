#pragma once

#include "envoy/common/pure.h"

#include "api/adaptive_load/adaptive_load.pb.h"

#include "absl/status/status.h"

namespace Nighthawk {

/**
 * Utilities for setting default values and validating user settings in the main
 * AdaptiveLoadSessionSpec proto.
 */
class AdaptiveLoadSessionSpecProtoHelper {
public:
  virtual ~AdaptiveLoadSessionSpecProtoHelper() = default;

  /**
   * Returns a copy of the input spec with default values inserted. Avoids overriding pre-set values
   * in the original spec.
   *
   * @param spec Valid adaptive load session spec.
   *
   * @return Adaptive load session spec with default values inserted.
   */
  virtual nighthawk::adaptive_load::AdaptiveLoadSessionSpec
  SetSessionSpecDefaults(nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec) const PURE;

  /**
   * Checks whether a session spec is valid: No forbidden fields in Nighthawk traffic spec; no bad
   * plugin references or bad plugin configurations (step controller, metric, scoring function); no
   * nonexistent metric names. Reports all errors in one pass.
   *
   * @param spec A potentially invalid adaptive load session spec.
   *
   * @return Status OK if no problems were found, or InvalidArgument with all errors.
   */
  virtual absl::Status
  CheckSessionSpec(const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) const PURE;
};

} // namespace Nighthawk