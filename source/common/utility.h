#pragma once

#include <string>

#include "absl/strings/string_view.h"

#include "envoy/stats/store.h"

#include "common/common/logger.h"
#include "common/network/dns_impl.h"
#include "common/network/utility.h"

#include "nighthawk/common/exception.h"

namespace Nighthawk {

namespace PlatformUtils {
uint32_t determineCpuCoresWithAffinity();
}

using StoreCounterFilter = std::function<bool(const std::string&, const uint64_t)>;

class Utility {
public:
  /**
   * Gets a map of tracked counter values, keyed by name.
   * @param filter function that returns true iff a counter should be included in the map,
   * based on the named and value it gets passed. The default filter returns all counters.
   * @return std::map<std::string, uint64_t> containing zero or more entries.
   */
  std::map<std::string, uint64_t> mapCountersFromStore(const Envoy::Stats::Store& store,
                                                       const StoreCounterFilter& filter =
                                                           [](const std::string&, const uint64_t) {
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
};

} // namespace Nighthawk