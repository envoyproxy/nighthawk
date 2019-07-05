#pragma once

#include <functional>
#include <memory>
#include <stack>
#include <vector>

#include "common/common/assert.h"
#include "nighthawk/common/exception.h"
#include "nighthawk/common/poolable.h"

namespace Nighthawk {

template <typename Poolable> class PoolImpl {
public:
  using PoolDeletionDelegate = std::function<void(Poolable*)>;
  using PoolInstanceConstructionDelegate = std::function<std::unique_ptr<Poolable>(void)>;
  using PoolInstanceResetDelegate = std::function<void(Poolable&)>;
  using PoolablePtr = std::unique_ptr<Poolable, PoolDeletionDelegate>;

  PoolImpl(PoolInstanceConstructionDelegate&& construction_delegate = nullptr,
           PoolInstanceResetDelegate&& reset_delegate = nullptr)
      : construction_delegate_(construction_delegate), reset_delegate_(reset_delegate) {}

  ~PoolImpl() {
    while (!pool_.empty()) {
      Poolable* poolable = pool_.top().get();
      all_.erase(std::remove(all_.begin(), all_.end(), poolable), all_.end());
      pool_.pop();
    }
    // Inform the in-flight poolables that they are own their own now.
    for (auto poolable : all_) {
      poolable->mark_orphaned();
    }
  }

  void addPoolable(std::unique_ptr<Poolable>&& poolable) {
    ASSERT(poolable.get() != nullptr);
    all_.push_back(poolable.get());
    pool_.push(std::move(poolable));
  }

  PoolablePtr get() {
    if (pool_.empty()) {
      if (construction_delegate_ != nullptr) {
        addPoolable(std::move(construction_delegate_()));
      } else {
        throw NighthawkException("Pool is out of resources");
      }
    }
    PoolablePtr poolable(pool_.top().release(),
                         [this](Poolable* poolable) { recyclePoolable(poolable); });
    pool_.pop();
    return poolable;
  }

  size_t available() const { return pool_.size(); }
  size_t allocated() const { return all_.size(); }

private:
  void recyclePoolable(Poolable* poolable) {
    if (!poolable->is_orphaned()) {
      if (reset_delegate_ != nullptr) {
        reset_delegate_(*poolable);
      }
      pool_.push(std::unique_ptr<Poolable>(poolable));
    } else {
      // The pool is gone, we must self-destruct.
      delete poolable;
    }
  }

  std::stack<std::unique_ptr<Poolable>> pool_;
  std::vector<Poolable*> all_;
  PoolInstanceConstructionDelegate construction_delegate_;
  PoolInstanceResetDelegate reset_delegate_;
};

} // namespace Nighthawk
