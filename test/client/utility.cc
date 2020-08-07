#include "test/client/utility.h"

#include "external/envoy/test/test_common/utility.h"

namespace Nighthawk {

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
  // This works around an error thrown by TCLAP about multiple unlabeled optional args not being
  // allowed. TCLAP has a global flag that detects multiple unlabeled optional args. It assumes
  // there will be only one command line in the lifetime of the process. In unit tests we parse
  // multiple TCLAP command lines, so we need to reset TCLAP's flag to simulate a fresh process.
  TCLAP::OptionalUnlabeledTracker::alreadyOptional() = false;

  return std::make_unique<OptionsImpl>(argv.size(), argv.data());
}

} // namespace Nighthawk
