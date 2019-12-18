#pragma once

#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Versioning {

constexpr int MAJOR_VERSION{0};
constexpr int MINOR_VERSION{3};
constexpr int PATCH_VERSION{0};

static std::string VersionString() {
  return absl::StrCat(MAJOR_VERSION, ".", MINOR_VERSION, ".", PATCH_VERSION);
}

} // namespace Versioning
} // namespace Nighthawk
