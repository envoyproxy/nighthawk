#include "common/utility.h"

#include "nighthawk/common/exception.h"

#include "common/http/utility.h"
#include "common/network/utility.h"

#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace Nighthawk {

namespace PlatformUtils {

// returns 0 on failure. returns the number of HW CPU's
// that the current thread has affinity with.
// TODO(oschaaf): mull over what to do w/regard to hyperthreading.
uint32_t determineCpuCoresWithAffinity() {
  const pthread_t thread = pthread_self();
  cpu_set_t cpuset;
  int i;

  CPU_ZERO(&cpuset);
  i = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (i == 0) {
    return CPU_COUNT(&cpuset);
  }
  return 0;
}

} // namespace PlatformUtils

std::map<std::string, uint64_t>
Utility::mapCountersFromStore(const Envoy::Stats::Store& store,
                              const StoreCounterFilter& filter) const {
  std::map<std::string, uint64_t> results;

  for (const auto& stat : store.counters()) {
    if (filter(stat->name(), stat->value())) {
      results[stat->name()] = stat->value();
    }
  }

  return results;
}

size_t Utility::findPortSeparator(absl::string_view hostname) {
  if (hostname.size() > 0 && hostname[0] == '[') {
    return hostname.find(":", hostname.find(']'));
  }
  return hostname.rfind(":");
}

Envoy::Network::DnsLookupFamily
Utility::translateFamilyOptionString(nighthawk::client::AddressFamily::AddressFamilyOptions value) {
  switch (value) {
  case nighthawk::client::AddressFamily_AddressFamilyOptions_v4:
    return Envoy::Network::DnsLookupFamily::V4Only;
  case nighthawk::client::AddressFamily_AddressFamilyOptions_v6:;
    return Envoy::Network::DnsLookupFamily::V6Only;
  case nighthawk::client::AddressFamily_AddressFamilyOptions_auto_:;
    return Envoy::Network::DnsLookupFamily::Auto;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

void Utility::parseCommand(TCLAP::CmdLine& cmd, const int argc, const char* const* argv) {
  cmd.setExceptionHandling(false);
  try {
    cmd.parse(argc, argv);
  } catch (TCLAP::ArgException& e) {
    try {
      cmd.getOutput()->failure(cmd, e);
    } catch (const TCLAP::ExitException&) {
      // failure() has already written an informative message to stderr, so all that's left to do
      // is throw our own exception with the original message.
      throw Client::MalformedArgvException(e.what());
    }
  } catch (const TCLAP::ExitException& e) {
    // parse() throws an ExitException with status 0 after printing the output for --help and
    // --version.
    throw Client::NoServingException();
  }
}

} // namespace Nighthawk