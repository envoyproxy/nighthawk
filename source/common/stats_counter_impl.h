#pragma once

namespace Nighthawk {

/**
 * @brief Used to wrap the stock counter implementation. We do this because we want to track
 * per-worker counter accumulations on top of just the global aggregated value. We can't just derive
 * from the stock CounterImpl because we need to go through
 * Envoy::Stats::AllocatorImpl::makeCounter. So we store that as an 'inner counter', and 1:1 proxy
 * most calls to it except for: add(), inc(), and value(). We specialize those to add tracking of
 * per-thread accumulations, and querying per-thread values.
 */
class NighthawkCounterImpl : public Envoy::Stats::Counter {
public:
  NighthawkCounterImpl(Envoy::Stats::CounterSharedPtr&& inner_counter)
      : inner_counter_(std::move(inner_counter)) {}

  std::string name() const override { return inner_counter_->name(); };
  Envoy::Stats::StatName statName() const override { return inner_counter_->statName(); };
  std::vector<Envoy::Stats::Tag> tags() const override { return inner_counter_->tags(); };
  std::string tagExtractedName() const override { return inner_counter_->tagExtractedName(); };
  Envoy::Stats::StatName tagExtractedStatName() const override {
    return inner_counter_->tagExtractedStatName();
  };
  void iterateTagStatNames(const Envoy::Stats::Counter::TagStatNameIterFn& fn) const override {
    inner_counter_->iterateTagStatNames(fn);
  };
  void iterateTags(const Envoy::Stats::Counter::TagIterFn& fn) const override {
    inner_counter_->iterateTags(fn);
  };
  bool used() const override { return inner_counter_->used(); };
  void add(uint64_t amount) override {
    auto pair = per_thread_counters_.insert({std::this_thread::get_id(), 0});
    pair.first->second += amount;
    inner_counter_->add(amount);
  };
  void inc() override { add(1); };
  uint64_t latch() override { return inner_counter_->latch(); };
  void reset() override { inner_counter_->reset(); };

  // We return the value we accumulated on this thread, if we have it.
  // Otherwise we return the value from the inner counter, which will have the
  // global value.
  uint64_t value() const override {
    auto it = per_thread_counters_.find(std::this_thread::get_id());
    if (it != per_thread_counters_.end()) {
      return it->second;
    } else {
      return inner_counter_->value();
    }
  };
  Envoy::Stats::SymbolTable& symbolTable() override { return inner_counter_->symbolTable(); }
  const Envoy::Stats::SymbolTable& constSymbolTable() const override {
    return inner_counter_->constSymbolTable();
  }

  // RefcountInterface
  void incRefCount() override { refcount_helper_.incRefCount(); }
  bool decRefCount() override { return refcount_helper_.decRefCount(); }
  uint32_t use_count() const override { return refcount_helper_.use_count(); }

private:
  Envoy::Stats::CounterSharedPtr inner_counter_;
  Envoy::Stats::RefcountHelper refcount_helper_;
  std::map<std::thread::id, uint64_t> per_thread_counters_;
};

} // namespace Nighthawk