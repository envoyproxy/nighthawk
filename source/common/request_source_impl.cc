#include "common/request_source_impl.h"

#include "external/envoy/source/common/common/assert.h"

#include "common/request_impl.h"

namespace Nighthawk {

StaticRequestSourceImpl::StaticRequestSourceImpl(Envoy::Http::HeaderMapPtr&& header,
                                                 const uint64_t max_yields)
    : header_(std::move(header)), yields_left_(max_yields) {
  RELEASE_ASSERT(header_ != nullptr, "header can't equal nullptr");
}

RequestGenerator StaticRequestSourceImpl::get() {
  return [this]() -> RequestPtr {
    while (yields_left_--) {
      return std::make_unique<RequestImpl>(header_);
    }
    return nullptr;
  };
}

} // namespace Nighthawk