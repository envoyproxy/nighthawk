#include "client/factories_impl.h"

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"

#include "common/header_source_impl.h"
#include "common/platform_util_impl.h"
#include "common/rate_limiter_impl.h"
#include "common/sequencer_impl.h"
#include "common/statistic_impl.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"
#include "client/output_collector_impl.h"

namespace Nighthawk {
namespace Client {

OptionBasedFactoryImpl::OptionBasedFactoryImpl(const Options& options) : options_(options) {}

BenchmarkClientFactoryImpl::BenchmarkClientFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

BenchmarkClientPtr BenchmarkClientFactoryImpl::create(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
    Envoy::Upstream::ClusterManagerPtr& cluster_manager, Envoy::Tracing::HttpTracerPtr& http_tracer,
    absl::string_view cluster_name, HeaderSource& header_generator) const {
  StatisticFactoryImpl statistic_factory(options_);
  auto benchmark_client = std::make_unique<BenchmarkClientHttpImpl>(
      api, dispatcher, scope, statistic_factory.create(), statistic_factory.create(), options_.h2(),
      cluster_manager, http_tracer, cluster_name, header_generator.get());
  auto request_options = options_.toCommandLineOptions()->request_options();
  benchmark_client->setConnectionLimit(options_.connections());
  benchmark_client->setMaxPendingRequests(options_.maxPendingRequests());
  benchmark_client->setMaxActiveRequests(options_.maxActiveRequests());
  benchmark_client->setMaxRequestsPerConnection(options_.maxRequestsPerConnection());
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
  SequencerTarget sequencer_target = [&benchmark_client](CompletionCallback f) -> bool {
    return benchmark_client.tryStartRequest(std::move(f));
  };
  return std::make_unique<SequencerImpl>(
      platform_util_, dispatcher, time_source, start_time, std::move(rate_limiter),
      sequencer_target, statistic_factory.create(), statistic_factory.create(), options_.duration(),
      options_.timeout(), options_.sequencerIdleStrategy());
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
  switch (options_.outputFormat()) {
  case nighthawk::client::OutputFormat::HUMAN:
    return std::make_unique<Client::ConsoleOutputCollectorImpl>(time_source_, options_);
  case nighthawk::client::OutputFormat::JSON:
    return std::make_unique<Client::JsonOutputCollectorImpl>(time_source_, options_);
  case nighthawk::client::OutputFormat::YAML:
    return std::make_unique<Client::YamlOutputCollectorImpl>(time_source_, options_);
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

HeaderSourceFactoryImpl::HeaderSourceFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

void HeaderSourceFactoryImpl::setRequestHeader(Envoy::Http::HeaderMap& header,
                                               absl::string_view key,
                                               absl::string_view value) const {
  auto lower_case_key = Envoy::Http::LowerCaseString(std::string(key));
  header.remove(lower_case_key);
  // TODO(oschaaf): we've performed zero validation on the header key/value.
  header.addCopy(lower_case_key, std::string(value));
}

HeaderSourcePtr HeaderSourceFactoryImpl::create(Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                                Envoy::Event::Dispatcher& dispatcher,
                                                Envoy::Stats::Scope& scope,
                                                absl::string_view service_cluster_name) const {
  // Note: we assume a valid uri.
  // Also, we can't resolve, but we do not need that.
  UriImpl uri(options_.uri());
  Envoy::Http::HeaderMapPtr header = std::make_unique<Envoy::Http::HeaderMapImpl>();

  header->insertMethod().value(envoy::api::v2::core::RequestMethod_Name(options_.requestMethod()));
  header->insertPath().value(uri.path());
  header->insertHost().value(uri.hostAndPort());
  header->insertScheme().value(uri.scheme() == "https"
                                   ? Envoy::Http::Headers::get().SchemeValues.Https
                                   : Envoy::Http::Headers::get().SchemeValues.Http);
  const uint32_t content_length = options_.requestBodySize();
  if (content_length > 0) {
    header->insertContentLength().value(content_length);
  }

  auto request_options = options_.toCommandLineOptions()->request_options();
  for (const auto& option_header : request_options.request_headers()) {
    setRequestHeader(*header, option_header.header().key(), option_header.header().value());
  }

  if (options_.headerSource() == "") {
    return std::make_unique<StaticHeaderSourceImpl>(std::move(header));
  } else {
    RELEASE_ASSERT(!service_cluster_name.empty(), "expected cluster name to be set");
    // We pass in options_.requestsPerSecond() as the header buffer length so the grpc client
    // will shoot for maintaining an amount of headers of at least one second.
    return std::make_unique<RemoteHeaderSourceImpl>(cluster_manager, dispatcher, scope,
                                                    service_cluster_name, std::move(header),
                                                    options_.requestsPerSecond());
  }
}

} // namespace Client
} // namespace Nighthawk
