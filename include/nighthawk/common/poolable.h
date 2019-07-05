#pragma once

#include "envoy/common/pure.h"

#include "common/common/non_copyable.h"

namespace Nighthawk {

class Poolable : Envoy::NonCopyable {
public:
  virtual ~Poolable() = default;
  /**
   * Marks the Poolable instance as orphaned. Called when the associated Pool destructs.
   */
  virtual void mark_orphaned() PURE;
  /**
   * @return bool true iff mark_orphaned() has been called.
   */
  virtual bool is_orphaned() const PURE;
};

} // namespace Nighthawk
