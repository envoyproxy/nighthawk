#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/client/header_generator.h"

namespace Nighthawk {
namespace Client {

class StaticHeaderGeneratorImpl : public HeaderGenerator {
public:
  StaticHeaderGeneratorImpl(Envoy::Http::HeaderMapPtr&&, const uint64_t max_yields = UINT64_MAX);
  GeneratorSignature get() override;

private:
  const HeaderMapPtr header_;
  uint64_t yields_left_;
};

} // namespace Client
} // namespace Nighthawk
