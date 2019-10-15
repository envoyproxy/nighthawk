#pragma once

#include <string>

#include "external/envoy/test/test_common/environment.h"

namespace Nighthawk {

// For now we delegate most things 1:1 to Envoy::TestEnvironment.
// We hide the original versions of runfilesDirectory() & runfilesPath(),
// and automagically fix that the right workspace gets passed in.
class TestEnvironment : public Envoy::TestEnvironment {
public:
  static std::string runfilesDirectory() {
    return Envoy::TestEnvironment::runfilesDirectory("nighthawk");
  }

  static std::string runfilesPath(const std::string& path) {
    return Envoy::TestEnvironment::runfilesPath(path, "nighthawk");
  }
};

} // namespace Nighthawk
