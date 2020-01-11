#include "client/factories_impl.h"

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"

#include "api/client/options.pb.h"

#include "common/platform_util_impl.h"
#include "common/rate_limiter_impl.h"
#include "common/request_source_impl.h"
#include "common/sequencer_impl.h"
#include "common/statistic_impl.h"
#include "common/termination_predicate_impl.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"
#include "client/output_collector_impl.h"
#include "client/output_formatter_impl.h"

namespace Nighthawk {
namespace Client {

OptionBasedFactoryImpl::OptionBasedFactoryImpl(const Options& options) : options_(options) {}

BenchmarkClientFactoryImpl::BenchmarkClientFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

BenchmarkClientPtr BenchmarkClientFactoryImpl::create(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
    Envoy::Upstream::ClusterManagerPtr& cluster_manager, Envoy::Tracing::HttpTracerPtr& http_tracer,
    absl::string_view cluster_name, RequestSource& request_generator) const {
  StatisticFactoryImpl statistic_factory(options_);
  // While we lack options to configure which statistic backend goes where, we directly pass
  // StreamingStatistic for the stats that track response sizes. Ideally we would have options
  // for this to route the right stat to the right backend (HdrStatistic, SimpleStatistic,
  // NullStatistic).
  // TODO(#XXX): Create options and have the StatisticFactory consider those when instantiating
  // statistics.
  auto benchmark_client = std::make_unique<BenchmarkClientHttpImpl>(
      api, dispatcher, scope, statistic_factory.create(), statistic_factory.create(),
      std::make_unique<StreamingStatistic>(), std::make_unique<StreamingStatistic>(), options_.h2(),
      cluster_manager, http_tracer, cluster_name, request_generator.get(), !options_.openLoop());
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
                                          BenchmarkClient& benchmark_client,
                                          TerminationPredicate& termination_predicate,
                                          Envoy::Stats::Scope& scope) const {
  StatisticFactoryImpl statistic_factory(options_);
  RateLimiterPtr rate_limiter =
      std::make_unique<LinearRateLimiter>(time_source, Frequency(options_.requestsPerSecond()));
  const uint64_t burst_size = options_.burstSize();

  if (burst_size) {
    rate_limiter = std::make_unique<BurstingRateLimiter>(std::move(rate_limiter), burst_size);
  }

  const std::chrono::nanoseconds jitter_uniform = options_.jitterUniform();
  if (jitter_uniform.count() > 0) {
    rate_limiter = std::make_unique<DistributionSamplingRateLimiterImpl>(
        std::make_unique<UniformRandomDistributionSamplerImpl>(jitter_uniform.count()),
        std::move(rate_limiter));
  }

  SequencerTarget sequencer_target = [&benchmark_client](CompletionCallback f) -> bool {
    return benchmark_client.tryStartRequest(std::move(f));
  };
  return std::make_unique<SequencerImpl>(
      platform_util_, dispatcher, time_source, start_time, std::move(rate_limiter),
      sequencer_target, statistic_factory.create(), statistic_factory.create(),
      options_.sequencerIdleStrategy(), termination_predicate, scope);
}

StoreFactoryImpl::StoreFactoryImpl(const Options& options) : OptionBasedFactoryImpl(options) {}

Envoy::Stats::StorePtr StoreFactoryImpl::create() const {
  return std::make_unique<Envoy::Stats::IsolatedStoreImpl>();
}

StatisticFactoryImpl::StatisticFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

StatisticPtr StatisticFactoryImpl::create() const { return std::make_unique<HdrStatistic>(); }

OutputFormatterPtr OutputFormatterFactoryImpl::create(
    const nighthawk::client::OutputFormat_OutputFormatOptions output_format) const {
  switch (output_format) {
  case nighthawk::client::OutputFormat::HUMAN:
    return std::make_unique<Client::ConsoleOutputFormatterImpl>();
  case nighthawk::client::OutputFormat::JSON:
    return std::make_unique<Client::JsonOutputFormatterImpl>();
  case nighthawk::client::OutputFormat::YAML:
    return std::make_unique<Client::YamlOutputFormatterImpl>();
  case nighthawk::client::OutputFormat::DOTTED:
    return std::make_unique<Client::DottedStringOutputFormatterImpl>();
  case nighthawk::client::OutputFormat::FORTIO:
    return std::make_unique<Client::FortioOutputFormatterImpl>();
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

RequestSourceFactoryImpl::RequestSourceFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

void RequestSourceFactoryImpl::setRequestHeader(Envoy::Http::HeaderMap& header,
                                                absl::string_view key,
                                                absl::string_view value) const {
  auto lower_case_key = Envoy::Http::LowerCaseString(std::string(key));
  header.remove(lower_case_key);
  // TODO(oschaaf): we've performed zero validation on the header key/value.
  header.addCopy(lower_case_key, std::string(value));
}

RequestSourcePtr RequestSourceFactoryImpl::create() const {
  Envoy::Http::HeaderMapPtr header = std::make_unique<Envoy::Http::HeaderMapImpl>();
  if (options_.uri().has_value()) {
    // We set headers based on the URI, but we don't have all the prerequisites to call the
    // resolver to validate the address at this stage. Resolving is performed during a later stage
    // and it will fail if the address is incorrect.
    UriImpl uri(options_.uri().value());
    header->setPath(uri.path());
    header->setHost(uri.hostAndPort());
    header->setScheme(uri.scheme() == "https" ? Envoy::Http::Headers::get().SchemeValues.Https
                                              : Envoy::Http::Headers::get().SchemeValues.Http);
  } else {
    header->setPath(options_.multiTargetPath());
    header->setHost(
        "host-not-supported-in-multitarget-mode"); // We set a default here because Nighthawk Test
                                                   // Server fails when Host is not set. If you send
                                                   // traffic to non-Nighthawk Test Server backends,
                                                   // you can override this with a custom Host on
                                                   // the command line, provided that your system
                                                   // works with the same Host value sent to all
                                                   // backends.
    header->setScheme(options_.multiTargetUseHttps()
                          ? Envoy::Http::Headers::get().SchemeValues.Https
                          : Envoy::Http::Headers::get().SchemeValues.Http);
  }

  header->setMethod(envoy::api::v2::core::RequestMethod_Name(options_.requestMethod()));
  const uint32_t content_length = options_.requestBodySize();
  if (content_length > 0) {
    header->setContentLength(content_length);
  }

  auto request_options = options_.toCommandLineOptions()->request_options();
  for (const auto& option_header : request_options.request_headers()) {
    setRequestHeader(*header, option_header.header().key(), option_header.header().value());
  }

  return std::make_unique<StaticRequestSourceImpl>(std::move(header));
}

TerminationPredicateFactoryImpl::TerminationPredicateFactoryImpl(const Options& options)
    : OptionBasedFactoryImpl(options) {}

TerminationPredicatePtr
TerminationPredicateFactoryImpl::create(Envoy::TimeSource& time_source, Envoy::Stats::Scope& scope,
                                        const Envoy::MonotonicTime start) const {
  TerminationPredicatePtr duration_predicate =
      std::make_unique<DurationTerminationPredicateImpl>(time_source, start, options_.duration());
  TerminationPredicate* current_predicate = duration_predicate.get();
  current_predicate = linkConfiguredPredicates(*current_predicate, options_.failurePredicates(),
                                               TerminationPredicate::Status::FAIL, scope);
  linkConfiguredPredicates(*current_predicate, options_.terminationPredicates(),
                           TerminationPredicate::Status::TERMINATE, scope);

  return duration_predicate;
}

TerminationPredicate* TerminationPredicateFactoryImpl::linkConfiguredPredicates(
    TerminationPredicate& last_predicate, const TerminationPredicateMap& predicates,
    const TerminationPredicate::Status termination_status, Envoy::Stats::Scope& scope) const {
  RELEASE_ASSERT(termination_status != TerminationPredicate::Status::PROCEED,
                 "PROCEED was unexpected");
  TerminationPredicate* current_predicate = &last_predicate;
  for (const auto& predicate : predicates) {
    ENVOY_LOG(trace, "Adding {} predicate for {} with threshold {}",
              termination_status == TerminationPredicate::Status::TERMINATE ? "termination"
                                                                            : "failure",
              predicate.first, predicate.second);
    current_predicate = &current_predicate->link(
        std::make_unique<StatsCounterAbsoluteThresholdTerminationPredicateImpl>(
            scope.counter(predicate.first), predicate.second, termination_status));
  }
  return current_predicate;
}

} // namespace Client
} // namespace Nighthawk
