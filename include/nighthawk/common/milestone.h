
#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "external/envoy/source/common/common/non_copyable.h"

namespace Nighthawk {

class Milestone {
public:
  Milestone() = default;
  virtual ~Milestone() = default;

  // Disallow copy, allow move.
  // NOTE: Envoy::NonCopyable also disallows move, hence we DIY here.
  Milestone(const Milestone&) = delete;
  Milestone& operator=(const Milestone&) = delete;
  Milestone(Milestone&&) noexcept = default;
  Milestone& operator=(Milestone&&) noexcept = default;

  virtual const Envoy::MonotonicTime& time() const PURE;
  virtual const char* name() const PURE;
};

using MilestonePtr = std::unique_ptr<Milestone>;
using MilestoneCollection = std::vector<MilestonePtr>;
class MilestoneTracker : Envoy::NonCopyable {
public:
  virtual ~MilestoneTracker() = default;
  virtual void addMilestone(const char* name) PURE;
};

using MilestoneCallback = std::function<void(const MilestoneCollection&)>;

} // namespace Nighthawk