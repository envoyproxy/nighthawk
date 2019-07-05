#include "common/poolable_impl.h"

namespace Nighthawk {

void PoolableImpl::mark_orphaned() override {
  ASSERT(!is_orphaned_);
  is_orphaned_ = true;
};

} // namespace Nighthawk
