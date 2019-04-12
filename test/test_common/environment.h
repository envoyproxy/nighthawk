#pragma once

#include <string>

#include "test/test_common/environment.h"

namespace Nighthawk {

// For now we delegate everything 1:1 to Envoy::TestEnvironment.
class TestEnvironment : public Envoy::TestEnvironment {};

} // namespace Nighthawk
