#include "client/header_generator_impl.h"

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {
namespace Client {

StaticHeaderGeneratorImpl::StaticHeaderGeneratorImpl(Envoy::Http::HeaderMapPtr&& header,
                                                     const uint64_t max_yields)
    : header_(std::move(header)), yields_left_(max_yields) {
  RELEASE_ASSERT(header_ != nullptr, "header can't equal nullptr");
}

GeneratorSignature StaticHeaderGeneratorImpl::get() {
  return [this]() -> HeaderMapPtr {
    while (yields_left_--) {
      return header_;
    }
    return nullptr;
  };
}

} // namespace Client
} // namespace Nighthawk