#pragma once

#include <functional>

#include "envoy/http/header_map.h"

namespace Nighthawk {
namespace Client {

using HeaderMapPtr = std::shared_ptr<const Envoy::Http::HeaderMap>;
using GeneratorSignature = std::function<HeaderMapPtr()>;

class HeaderGenerator {
public:
  virtual ~HeaderGenerator() = default;
  virtual GeneratorSignature get() PURE;
};

using HeaderGeneratorPtr = std::unique_ptr<HeaderGenerator>;

} // namespace Client
} // namespace Nighthawk
