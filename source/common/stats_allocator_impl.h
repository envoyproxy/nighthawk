#pragma once

#include "common/stats/allocator_impl.h"
#include "common/stats_counter_impl.h"

namespace Nighthawk {

/**
 * @brief Our custom allocator overrides counter creation.
 */
class StatsAllocatorImpl : public Envoy::Stats::AllocatorImpl {
public:
  using Envoy::Stats::AllocatorImpl::AllocatorImpl;

  /**
   * @return Envoy::Stats::CounterSharedPtr Containing our wrapper which has the result
   * of makeCounter from the base class as an inner counter.
   * Note that we rely on the caching properties of the tls-store to ensure that we'll
   * only have a single instance per statname.
   */
  Envoy::Stats::CounterSharedPtr makeCounter(Envoy::Stats::StatName name,
                                             absl::string_view tag_extracted_name,
                                             const std::vector<Envoy::Stats::Tag>& tags) override {
    auto counter = Envoy::Stats::AllocatorImpl::makeCounter(name, tag_extracted_name, tags);
    return Envoy::Stats::CounterSharedPtr(new NighthawkCounterImpl(std::move(counter)));
  }
};

} // namespace Nighthawk
