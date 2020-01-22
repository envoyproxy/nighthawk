#pragma once

#include "envoy/config/core/v3/base.pb.h"

#define NIGHTHAWK_BUILD_VERSION_NUMBER "0.3.0"

namespace Nighthawk {

/*
 * TODO(#267): This class is heavily based on Envoy's source/common/common/version.h
 * There's some code duplication going on as we cannot easily directly fully
 * re-use it. It would be great if we can make that happen.
 */
class VersionInfo {
public:
  /**
   * @return const std::string& a representation of the current version.
   */
  static const std::string& version();
  /**
   * @return const envoy::config::core::v3::BuildVersion& a representation of the current
   * version.
   */
  static const envoy::config::core::v3::BuildVersion& buildVersion();
  /**
   * @brief Transforms a proto representation of a build version into a string representation.
   *
   * @param build_version proto build-version input that should be transformed.
   * @return const std::string representation of the transformed proto input.
   */
  static const std::string
  toVersionString(const envoy::config::core::v3::BuildVersion& build_version);

private:
  static envoy::config::core::v3::BuildVersion makeBuildVersion(const char* version);
};

} // namespace Nighthawk
