#pragma once

#include "nighthawk/common/poolable.h"

#include "common/common/assert.h"

namespace Nighthawk {

class PoolableImpl : public Poolable {
public:
  void mark_orphaned() override;
  bool is_orphaned() const override { return is_orphaned_; };

private:
  bool is_orphaned_{false};
};

} // namespace Nighthawk
