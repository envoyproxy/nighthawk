#pragma once

#include "absl/strings/str_cat.h"

namespace Nighthawk {

const int MAJOR_VERSION{0};
const int MINOR_VERSION{3};
const int PATCH_VERSION{0};

class VersionUtils {
public:
  static std::string VersionString() {
    return absl::StrCat(MAJOR_VERSION, ".", MINOR_VERSION, ".", PATCH_VERSION);
  }
};

} // namespace Nighthawk
