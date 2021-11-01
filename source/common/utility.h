#pragma once

#include <string>

#include "envoy/stats/store.h"

#include "nighthawk/common/exception.h"

#include "external/envoy/envoy/network/dns.h"

#include "api/client/options.pb.h"

#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {

using StoreCounterFilter = std::function<bool(absl::string_view, const uint64_t)>;

class Utility {
public:
  /**
   * Gets a map of tracked counter values, keyed by name.
   * @param filter function that returns true iff a counter should be included in the map,
   * based on the named and value it gets passed. The default filter returns all counters.
   * @return std::map<std::string, uint64_t> containing zero or more entries.
   */
  std::map<std::string, uint64_t> mapCountersFromStore(
      const Envoy::Stats::Store& store,
      const StoreCounterFilter& filter = [](absl::string_view, const uint64_t) {
        return true;
      }) const;
  /**
   * Finds the position of the port separator in the host:port fragment.
   *
   * @param hostname valid "host[:port]" string.
   * @return size_t the position of the port separator, or absl::string_view::npos if none was
   * found.
   */
  static size_t findPortSeparator(absl::string_view hostname);

  /**
   * @param family Address family as a string. Allowed values are "v6", "v4", and "auto" (case
   * insensitive). Any other values will throw a NighthawkException.
   * @return Envoy::Network::DnsLookupFamily the equivalent DnsLookupFamily value
   */
  static Envoy::Network::DnsLookupFamily
  translateFamilyOptionString(nighthawk::client::AddressFamily::AddressFamilyOptions value);

  /**
   * Executes TCLAP command line parsing
   * @param cmd TCLAP command line specification.
   * @param argc forwarded argc argument of the main entry point.
   * @param argv forwarded argv argument of the main entry point.
   */
  static void parseCommand(TCLAP::CmdLine& cmd, const int argc, const char* const* argv);

  /**
   * @param host_port host:port as a string, where host can be IPv4, [IPv6], or a DNS
   * name.
   * @param host string* to receive the host if the parse succeeds
   * @param port int* to receive the port if the parse succeeds
   * @return bool true if the input could be parsed as host:port
   */
  static bool parseHostPort(const std::string& host_port, std::string* host, int* port);
};

} // namespace Nighthawk
