#include "nighthawk/source/client/factories_impl.h"

#include "common/stats/isolated_store_impl.h"

#include "nighthawk/source/client/benchmark_client_impl.h"
#include "nighthawk/source/client/output_formatter_impl.h"
#include "nighthawk/source/common/platform_util_impl.h"
#include "nighthawk/source/common/rate_limiter_impl.h"
#include "nighthawk/source/common/sequencer_impl.h"
#include "nighthawk/source/common/statistic_impl.h"
#include "nighthawk/source/common/utility.h"

namespace Nighthawk {
namespace Client {

OptionBasedFactoryImpl::OptionBasedFactoryImpl(const Options& options) : options_(options) {}

BenchmarkClientFactoryImpl::BenchmarkClientFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

BenchmarkClientPtr BenchmarkClientFactoryImpl::create(Envoy::Api::Api& api,
                                                      Envoy::Event::Dispatcher& dispatcher,
                                                      Envoy::Stats::Store& store,
                                                      const Uri uri) const {
  StatisticFactoryImpl statistic_factory(options_);
  auto benchmark_client =
      std::make_unique<BenchmarkClientHttpImpl>(api, dispatcher, store, statistic_factory.create(),
                                                statistic_factory.create(), uri, options_.h2());
  benchmark_client->setConnectionTimeout(options_.timeout());
  benchmark_client->setConnectionLimit(options_.connections());
  return benchmark_client;
}

SequencerFactoryImpl::SequencerFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

SequencerPtr SequencerFactoryImpl::create(Envoy::TimeSource& time_source,
                                          Envoy::Event::Dispatcher& dispatcher,
                                          Envoy::MonotonicTime start_time,
                                          BenchmarkClient& benchmark_client) const {
  StatisticFactoryImpl statistic_factory(options_);
  RateLimiterPtr rate_limiter =
      std::make_unique<LinearRateLimiter>(time_source, Frequency(options_.requestsPerSecond()));
  SequencerTarget sequencer_target = [&benchmark_client](std::function<void()> f) -> bool {
    return benchmark_client.tryStartOne(f);
  };
  return std::make_unique<SequencerImpl>(platform_util_, dispatcher, time_source, start_time,
                                         std::move(rate_limiter), sequencer_target,
                                         statistic_factory.create(), statistic_factory.create(),
                                         options_.duration(), options_.timeout());
}

StoreFactoryImpl::StoreFactoryImpl(const Options& options) : OptionBasedFactoryImpl(options) {}

Envoy::Stats::StorePtr StoreFactoryImpl::create() const {
  return std::make_unique<Envoy::Stats::IsolatedStoreImpl>();
}

StatisticFactoryImpl::StatisticFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

StatisticPtr StatisticFactoryImpl::create() const { return std::make_unique<HdrStatistic>(); }

OutputFormatterFactoryImpl::OutputFormatterFactoryImpl(Envoy::TimeSource& time_source,
                                                       const Options& options)
    : OptionBasedFactoryImpl(options), time_source_(time_source) {}

OutputFormatterPtr OutputFormatterFactoryImpl::create() const {
  const std::string format = options_.outputFormat();
  if (format == "human") {
    return std::make_unique<Client::ConsoleOutputFormatterImpl>(time_source_, options_);
  } else if (format == "json") {
    return std::make_unique<Client::JsonOutputFormatterImpl>(time_source_, options_);
  } else if (format == "yaml") {
    return std::make_unique<Client::YamlOutputFormatterImpl>(time_source_, options_);
  }
  NOT_REACHED_GCOVR_EXCL_LINE;
}

} // namespace Client
} // namespace Nighthawk
