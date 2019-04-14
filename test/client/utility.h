#pragma once

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"

#include "client/options_impl.h"

namespace Nighthawk {
namespace Client {

class TestUtility {
public:
  static std::unique_ptr<OptionsImpl> createOptionsImpl(absl::string_view args);

private:
  TestUtility() = default;
};

} // namespace Client
} // namespace Nighthawk