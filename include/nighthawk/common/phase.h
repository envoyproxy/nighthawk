
#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "envoy/common/pure.h"

#include "nighthawk/common/sequencer.h"

namespace Nighthawk {

class Phase {
public:
  virtual ~Phase() = default;

  virtual absl::string_view id() const PURE;

  virtual Sequencer& sequencer() const PURE;
};

using PhasePtr = std::unique_ptr<Phase>;

} // namespace Nighthawk