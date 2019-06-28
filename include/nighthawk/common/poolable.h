#pragma once

#include "envoy/common/pure.h"

namespace Nighthawk {

class Poolable {
public:
  virtual ~Poolable() = default;
  virtual void orphan() PURE;
  virtual bool orphaned() const PURE;
};

} // namespace Nighthawk
