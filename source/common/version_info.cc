#include "common/version_info.h"

#include <string>

#include "external/envoy/source/common/common/macros.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace Nighthawk {

const std::string& VersionInfo::version() {
  CONSTRUCT_ON_FIRST_USE(std::string, NIGHTHAWK_BUILD_VERSION_NUMBER);
}

const envoy::config::core::v3alpha::BuildVersion& VersionInfo::buildVersion() {
  CONSTRUCT_ON_FIRST_USE(envoy::config::core::v3alpha::BuildVersion,
                         makeBuildVersion(NIGHTHAWK_BUILD_VERSION_NUMBER));
}

envoy::config::core::v3alpha::BuildVersion VersionInfo::makeBuildVersion(const char* version) {
  // Rewritten from the Envoy version to avoid using std/regex.
  // TODO(#262): Add a generic means to Envoy's check_format.py to allow line-level
  // exclusion for this checking as an escape latch.
  std::vector<std::string> tmp = absl::StrSplit(version, '.');
  envoy::config::core::v3alpha::BuildVersion result;
  int major, minor, patch = -1;
  if (absl::SimpleAtoi(tmp[0], &major) && absl::SimpleAtoi(tmp[1], &minor) &&
      absl::SimpleAtoi(tmp[2], &patch)) {
    result.mutable_version()->set_major(major);
    result.mutable_version()->set_minor(minor);
    result.mutable_version()->set_patch(patch);
  }
  return result;
}

const std::string
VersionInfo::toVersionString(const envoy::config::core::v3alpha::BuildVersion& build_version) {
  const auto& version = build_version.version();
  return absl::StrCat(version.major(), ".", version.minor(), ".", version.patch());
}

} // namespace Nighthawk