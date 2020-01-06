#include "test/client/utility.h"

#include "external/envoy/test/test_common/utility.h"

namespace Nighthawk {
namespace Client {

std::unique_ptr<OptionsImpl> TestUtility::createOptionsImpl(absl::string_view args) {
  std::vector<std::string> words = Envoy::TestUtility::split(std::string(args), ' ');
  std::vector<const char*> argv;
  argv.reserve(words.size());
  for (const std::string& s : words) {
    argv.push_back(s.c_str());
  }
  return createOptionsImpl(argv);
}

std::unique_ptr<OptionsImpl> TestUtility::createOptionsImpl(const std::vector<const char*>& argv) {
  TCLAP::OptionalUnlabeledTracker::alreadyOptional() = false;
  return std::make_unique<OptionsImpl>(argv.size(), argv.data());
}

} // namespace Client
} // namespace Nighthawk
