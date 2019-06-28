#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <vector>

#include "nighthawk/common/poolable.h"

#include "common/common/assert.h"

namespace Nighthawk {

template <typename Poolable> class PoolImpl {
public:
  using poolDeletionDelegate = std::function<void(Poolable*)>;
  using PoolablePtr = std::unique_ptr<Poolable, poolDeletionDelegate>;

  ~PoolImpl() {
    while (!pool_.empty()) {
      Poolable* poolable = pool_.top().get();
      all_.erase(std::remove(all_.begin(), all_.end(), poolable), all_.end());
      pool_.pop();
    }
    // Inform the in-flight poolables that they are own their own now.
    for (auto poolable : all_) {
      poolable->orphan();
    }
  }

  void addPoolable(std::unique_ptr<Poolable> poolable) {
    all_.push_back(poolable.get());
    pool_.push(std::move(poolable));
  }

  PoolablePtr get() {
    if (pool_.empty()) {
      throw;
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
    if (!poolable->orphaned()) {
      pool_.push(std::unique_ptr<Poolable>(poolable));
    } else {
      // The pool is gone, we must self-destruct.
      delete poolable;
    }
  }

  std::stack<std::unique_ptr<Poolable>> pool_;
  std::vector<Poolable*> all_;
};

} // namespace Nighthawk
