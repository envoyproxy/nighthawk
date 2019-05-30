#include "client/factories_impl.h"

#include "common/platform_util_impl.h"
#include "common/rate_limiter_impl.h"
#include "common/sequencer_impl.h"
#include "common/statistic_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"
#include "client/output_collector_impl.h"

namespace Nighthawk {
namespace Client {

OptionBasedFactoryImpl::OptionBasedFactoryImpl(const Options& options) : options_(options) {}

BenchmarkClientFactoryImpl::BenchmarkClientFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

BenchmarkClientPtr BenchmarkClientFactoryImpl::create(Envoy::Api::Api& api,
                                                      Envoy::Event::Dispatcher& dispatcher,
                                                      Envoy::Stats::Store& store,
                                                      UriPtr&& uri) const {
  StatisticFactoryImpl statistic_factory(options_);
  auto benchmark_client = std::make_unique<BenchmarkClientHttpImpl>(
      api, dispatcher, store, statistic_factory.create(), statistic_factory.create(),
      std::move(uri), options_.h2(), options_.prefetchConnections());

  benchmark_client->setRequestMethod(options_.requestMethod());

  auto request_options = options_.toCommandLineOptions()->request_options();
  if (request_options.request_headers_size() > 0) {
    for (const auto& header : request_options.request_headers()) {
      benchmark_client->setRequestHeader(header.header().key(), header.header().value());
    }
  }

  benchmark_client->setRequestBodySize(options_.requestBodySize());
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
  const uint64_t burst_size = options_.burstSize();
  if (burst_size) {
    rate_limiter = std::make_unique<BurstingRateLimiter>(std::move(rate_limiter), burst_size);
  }
  SequencerTarget sequencer_target = [&benchmark_client](std::function<void()> f) -> bool {
    return benchmark_client.tryStartOne(std::move(f));
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

OutputCollectorFactoryImpl::OutputCollectorFactoryImpl(Envoy::TimeSource& time_source,
                                                       const Options& options)
    : OptionBasedFactoryImpl(options), time_source_(time_source) {}

OutputCollectorPtr OutputCollectorFactoryImpl::create() const {
  const std::string format = options_.outputFormat();
  if (format == "human") {
    return std::make_unique<Client::ConsoleOutputCollectorImpl>(time_source_, options_);
  } else if (format == "json") {
    return std::make_unique<Client::JsonOutputCollectorImpl>(time_source_, options_);
  } else if (format == "yaml") {
    return std::make_unique<Client::YamlOutputCollectorImpl>(time_source_, options_);
  }
  NOT_REACHED_GCOVR_EXCL_LINE;
}

} // namespace Client
} // namespace Nighthawk
