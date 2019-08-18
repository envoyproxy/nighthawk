#pragma once

#include "common/stats/allocator_impl.h"
#include "common/stats_counter_impl.h"

namespace Nighthawk {

/**
 * @brief Our custom allocator overrides counter creation, so we have an opportunity
 * to wrap the tls-counter that gets cached with our own CounterImpl wrapper.
 */
class StatsAllocatorImpl : public Envoy::Stats::AllocatorImpl {
public:
  using Envoy::Stats::AllocatorImpl::AllocatorImpl;

  Envoy::Stats::CounterSharedPtr makeCounter(Envoy::Stats::StatName name,
                                             absl::string_view tag_extracted_name,
                                             const std::vector<Envoy::Stats::Tag>& tags) override {
    auto counter = Envoy::Stats::AllocatorImpl::makeCounter(name, tag_extracted_name, tags);
    return Envoy::Stats::CounterSharedPtr(new NighthawkCounterImpl(std::move(counter)));
  }
};

} // namespace Nighthawk
