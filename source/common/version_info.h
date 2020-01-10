#pragma once

#include "envoy/config/core/v3alpha/base.pb.h"

#define NIGHTHAWK_BUILD_VERSION_NUMBER "0.3.0"

namespace Nighthawk {

/*
 * TODO(#267): This class is heavily based on Envoy's source/common/common/version.h
 * There's some code duplication going on as we cannot easily directly fully
 * re-use it. It would be great if we can make that happen.
 */
class VersionInfo {
public:
  static const std::string& version();
  static const envoy::config::core::v3alpha::BuildVersion& buildVersion();
  static const std::string
  toVersionString(const envoy::config::core::v3alpha::BuildVersion& build_version);

private:
  static envoy::config::core::v3alpha::BuildVersion makeBuildVersion(const char* version);
};

} // namespace Nighthawk
