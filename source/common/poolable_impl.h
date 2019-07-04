#pragma once

#include "nighthawk/common/poolable.h"

namespace Nighthawk {

class PoolableImpl : public Poolable {
public:
  void mark_orphaned() override {
    ASSERT(!is_orphaned_);
    is_orphaned_ = true;
  };
  bool is_orphaned() const override { return is_orphaned_; };

private:
  bool is_orphaned_{false};
};

} // namespace Nighthawk
