#pragma once

#include "common/stats/allocator_impl.h"

namespace Nighthawk {

/**
 * @brief Used to wrap the stock counter implementation. We do this because we want to track
 * per-worker counter accumulations on top of just the global aggregated value. We can't just derive
 * from the stock CounterImpl because we need to go through
 * Envoy::Stats::AllocatorImpl::makeCounter. So we store that as an 'inner counter', and 1:1 proxy
 * expected calls to it except for: add(), inc(), and value(). We specialize those to add tracking
 * of per-thread accumulations, and querying per-thread values. Once created, this counter will be
 * cached in the tls-cache when the tls-store is used. This won't work with the isolated store
 * implementation, but then it doesn't make much sense to use this with that. We have lots of
 * NOT_IMPLEMENTED_GCOVR_EXCL_LINE to make sure we hear about unexpected calls in the future, in
 * case implementation details change.
 */
class NighthawkCounterImpl : public Envoy::Stats::Counter {
public:
  NighthawkCounterImpl(Envoy::Stats::CounterSharedPtr&& inner_counter)
      : inner_counter_(std::move(inner_counter)) {}

  std::string name() const override { return inner_counter_->name(); };
  Envoy::Stats::StatName statName() const override { return inner_counter_->statName(); };
  std::vector<Envoy::Stats::Tag> tags() const override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  std::string tagExtractedName() const override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  Envoy::Stats::StatName tagExtractedStatName() const override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  void iterateTagStatNames(const Envoy::Stats::Counter::TagStatNameIterFn&) const override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  };
  void iterateTags(const Envoy::Stats::Counter::TagIterFn&) const override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  };
  bool used() const override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  void add(uint64_t amount) override {
    // TODO(oschaaf): Locking here isn't ideal. But it is fairly
    // granular, as it is done per-counter. Still, we're in the hot path
    // here, so look into ways to avoid it if we observe contention.
    // Ideally we'd preallocate thread-local slots for storage (and also eliminate the thread
    // lookup).
    mutex_.lock();
    auto pair = per_thread_counters_.emplace(std::this_thread::get_id(), amount);
    mutex_.unlock();
    if (!pair.second) {
      pair.first->second += amount;
    }
    inner_counter_->add(amount);
  };
  void inc() override { add(1); };
  uint64_t latch() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  void reset() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };

  // We return the value we accumulated on this thread, if we have it.
  // Otherwise we return the value from the inner counter, which will have the
  // global value.
  uint64_t value() const override {
    // XXX(oschaaf): I don't think this wil be called concurrently with
    // add(), but still it would be good to ensure std::map would be OK
    // with that to avoid future surprises. Note: we can't lock here, this
    // being a const method.
    const auto it = per_thread_counters_.find(std::this_thread::get_id());
    if (it != per_thread_counters_.end()) {
      return it->second;
    } else {
      return inner_counter_->value();
    }
  };
  Envoy::Stats::SymbolTable& symbolTable() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  const Envoy::Stats::SymbolTable& constSymbolTable() const override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  // RefcountInterface
  void incRefCount() override { refcount_helper_.incRefCount(); }
  bool decRefCount() override { return refcount_helper_.decRefCount(); }
  uint32_t use_count() const override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

private:
  Envoy::Thread::MutexBasicLockable mutex_;
  Envoy::Stats::CounterSharedPtr inner_counter_;
  Envoy::Stats::RefcountHelper refcount_helper_;
  std::map<std::thread::id, uint64_t> per_thread_counters_;
};

} // namespace Nighthawk