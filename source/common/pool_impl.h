#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "nighthawk/common/exception.h"
#include "nighthawk/common/poolable.h"

#include "common/common/assert.h"

namespace Nighthawk {

template <typename Poolable> class PoolImpl {
public:
  using PoolDeletionDelegate = std::function<void(Poolable*)>;
  using PoolInstanceConstructionDelegate = std::function<Poolable*(void)>;
  using PoolInstanceResetDelegate = std::function<void(Poolable&)>;
  using PoolablePtr = std::unique_ptr<Poolable, PoolDeletionDelegate>;

  PoolImpl(PoolInstanceConstructionDelegate&& construction_delegate = nullptr,
           PoolInstanceResetDelegate&& reset_delegate = nullptr)
      : construction_delegate_(construction_delegate), reset_delegate_(reset_delegate) {}

  ~PoolImpl() {
    while (!pool_.empty()) {
      Poolable* poolable = pool_.back().get();
      all_.erase(std::remove(all_.begin(), all_.end(), poolable), all_.end());
      pool_.pop_back();
    }
    // Inform the in-flight poolables that they are own their own now.
    for (auto poolable : all_) {
      poolable->mark_orphaned();
    }
  }

  void addPoolable(std::unique_ptr<Poolable>&& poolable) {
    ASSERT(poolable.get() != nullptr);
    all_.push_back(poolable.get());
    pool_.emplace_back(std::move(poolable));
  }

  PoolablePtr get() {
    PoolDeletionDelegate release_fn = [this](Poolable* poolable) {
      if (poolable->is_orphaned()) {
        delete poolable;
      } else {
        recyclePoolable(poolable);
      }
    };
    if (pool_.empty()) {
      if (construction_delegate_ != nullptr) {
        PoolablePtr poolable(construction_delegate_(), release_fn);
        all_.push_back(poolable.get());
        return poolable;
      } else {
        throw NighthawkException("Pool is out of resources");
      }
    }
    PoolablePtr poolable(pool_.back().release(), release_fn);
    pool_.pop_back();
    return poolable;
  }

  size_t available() const { return pool_.size(); }
  size_t allocated() const { return all_.size(); }

private:
  void recyclePoolable(Poolable* poolable) {
    if (reset_delegate_ != nullptr) {
      reset_delegate_(*poolable);
    }
    pool_.push_back(std::unique_ptr<Poolable>(poolable));
  }

  std::vector<std::unique_ptr<Poolable>> pool_;
  std::vector<Poolable*> all_;
  PoolInstanceConstructionDelegate construction_delegate_;
  PoolInstanceResetDelegate reset_delegate_;
};

} // namespace Nighthawk
