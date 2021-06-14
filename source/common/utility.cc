#include "source/common/utility.h"

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/utility.h"

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace Nighthawk {

std::map<std::string, uint64_t>
Utility::mapCountersFromStore(const Envoy::Stats::Store& store,
                              const StoreCounterFilter& filter) const {
  std::map<std::string, uint64_t> results;

  for (const auto& stat : store.counters()) {
    if (filter(stat->name(), stat->value())) {
      std::string stat_name = stat->name();
      // Strip off cluster.[x]. & worker.[x]. prefixes.
      std::vector<std::string> v = absl::StrSplit(stat_name, '.');
      if (v[0] == "cluster" || v[0] == "worker") {
        v.erase(v.begin());
      }
      int tmp;
      if (absl::SimpleAtoi(v[0], &tmp)) {
        v.erase(v.begin());
      }
      stat_name = absl::StrJoin(v, ".");
      results[stat_name] += stat->value();
    }
  }
  return results;
}

size_t Utility::findPortSeparator(absl::string_view hostname) {
  if (hostname.size() > 0 && hostname[0] == '[') {
    return hostname.find(':', hostname.find(']'));
  }
  return hostname.rfind(':');
}

Envoy::Network::DnsLookupFamily
Utility::translateFamilyOptionString(nighthawk::client::AddressFamily::AddressFamilyOptions value) {
  switch (value) {
  case nighthawk::client::AddressFamily_AddressFamilyOptions_V4:
    return Envoy::Network::DnsLookupFamily::V4Only;
  case nighthawk::client::AddressFamily_AddressFamilyOptions_V6:
    return Envoy::Network::DnsLookupFamily::V6Only;
  case nighthawk::client::AddressFamily_AddressFamilyOptions_AUTO:
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

bool Utility::parseHostPort(const std::string& host_port, std::string* address, int* port) {
  return RE2::FullMatch(host_port, R"((\d+\.\d+\.\d+\.\d+):(\d+))", address, port) ||
         RE2::FullMatch(host_port, R"((\[[.:0-9a-fA-F]+\]):(\d+))", address, port) ||
         RE2::FullMatch(host_port, R"(([-.0-9a-zA-Z]+):(\d+))", address, port);
}

} // namespace Nighthawk
