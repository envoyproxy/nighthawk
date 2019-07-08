#pragma once

#include "envoy/common/pure.h"

#include "common/common/non_copyable.h"

namespace Nighthawk {

/**
 * Poolable is an interface that allows PoolImpl<T> to interact with
 * objects that implement it (or inherit the generic PoolableImpl). Poolable objects allocated from
 * the pool will be accessible through a unique_ptr with a custom deleter. When the deleter runs
 * while the associated pool is alive, the poolable object will be recycled. The pool will mark any
 * in-use Poolable instances as orphaned during destruction. The custom deleter will delete
 * poolable objects which have been marked as orphaned.
 */
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
