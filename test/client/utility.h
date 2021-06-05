#pragma once

#include <memory>
#include <vector>

#include "source/client/options_impl.h"

#include "absl/strings/string_view.h"

namespace Nighthawk {
namespace Client {

class TestUtility {
public:
  // Create OptionsImpl from a concatenation of arguments delimited by space.
  // Use the overload below if any argument should contain embedded spaces.
  static std::unique_ptr<OptionsImpl> createOptionsImpl(absl::string_view args);

  // Create OptionsImpl from a vector of argument strings.
  static std::unique_ptr<OptionsImpl> createOptionsImpl(const std::vector<const char*>& args);

private:
  TestUtility() = default;
};

} // namespace Client
} // namespace Nighthawk
