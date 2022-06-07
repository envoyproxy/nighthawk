#include "source/server/http_test_server_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.validate.h"

#include "source/server/configuration.h"
#include "source/server/well_known_headers.h"

#include "absl/strings/numbers.h"

namespace Nighthawk {
namespace Server {
namespace {

using ::nighthawk::server::ResponseOptions;

const absl::StatusOr<std::shared_ptr<const ResponseOptions>>
computeEffectiveConfiguration(std::shared_ptr<const ResponseOptions> base_filter_config,
                              const Envoy::Http::RequestHeaderMap& request_headers) {
  const auto& request_config_header =
      request_headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header.size() == 1) {
    // We could be more flexible and look for the first request header that has a value,
    // but without a proper understanding of a real use case for that, we are assuming that any
    // existence of duplicate headers here is an error.
    ResponseOptions modified_filter_config = *base_filter_config;
    std::string error_message;
    if (Configuration::mergeJsonConfig(request_config_header[0]->value().getStringView(),
                                       modified_filter_config, error_message)) {
      return std::make_shared<const ResponseOptions>(std::move(modified_filter_config));
    } else {
      return absl::InvalidArgumentError(error_message);
    }
  } else if (request_config_header.size() > 1) {
    return absl::InvalidArgumentError(
        "Received multiple configuration headers in the request, expected only one.");
  }
  return base_filter_config;
}

} // namespace

HttpTestServerDecoderFilterConfig::HttpTestServerDecoderFilterConfig(
    const ResponseOptions& proto_config)
    : FilterConfigurationBase("test-server"),
      server_config_(std::make_shared<ResponseOptions>(proto_config)) {}

std::shared_ptr<const ResponseOptions>
HttpTestServerDecoderFilterConfig::getStartupFilterConfiguration() {
  return server_config_;
}

HttpTestServerDecoderFilter::HttpTestServerDecoderFilter(
    HttpTestServerDecoderFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

void HttpTestServerDecoderFilter::onDestroy() {}

void HttpTestServerDecoderFilter::sendReply(const ResponseOptions& options) {
  std::string response_body(options.response_body_size(), 'a');
  if (request_headers_dump_.has_value()) {
    response_body += *request_headers_dump_;
  }
  decoder_callbacks_->sendLocalReply(
      static_cast<Envoy::Http::Code>(200), response_body,
      [options](Envoy::Http::ResponseHeaderMap& direct_response_headers) {
        Configuration::applyConfigToResponseHeaders(direct_response_headers, options);
      },
      absl::nullopt, "");
}

Envoy::Http::FilterHeadersStatus
HttpTestServerDecoderFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers,
                                           bool end_stream) {
  effective_config_ =
      computeEffectiveConfiguration(config_->getStartupFilterConfiguration(), headers);
  if (end_stream) {
    if (!config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_)) {
      if (effective_config_.value()->echo_request_headers()) {
        std::stringstream headers_dump;
        headers_dump << "\nRequest Headers:\n" << headers;
        request_headers_dump_ = headers_dump.str();
      }
      sendReply(*effective_config_.value());
    }
  }
  return Envoy::Http::FilterHeadersStatus::StopIteration;
}

Envoy::Http::FilterDataStatus HttpTestServerDecoderFilter::decodeData(Envoy::Buffer::Instance&,
                                                                      bool end_stream) {
  if (end_stream) {
    if (!config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_)) {
      sendReply(*effective_config_.value());
    }
  }
  return Envoy::Http::FilterDataStatus::StopIterationNoBuffer;
}

Envoy::Http::FilterTrailersStatus
HttpTestServerDecoderFilter::decodeTrailers(Envoy::Http::RequestTrailerMap&) {
  return Envoy::Http::FilterTrailersStatus::Continue;
}

void HttpTestServerDecoderFilter::setDecoderFilterCallbacks(
    Envoy::Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Server
} // namespace Nighthawk
