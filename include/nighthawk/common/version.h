#pragma once

#include "external/envoy/source/common/common/fmt.h"

namespace Nighthawk {

const int MAJOR_VERSION{0};
const int MINOR_VERSION{3};

class Globals {
public:
  static std::string VersionString() { return fmt::format("{}.{}", MAJOR_VERSION, MINOR_VERSION); }
};

} // namespace Nighthawk
