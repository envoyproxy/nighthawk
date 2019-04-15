#include <string>

#include "server/http_test_server_filter.h"

#include "absl/strings/numbers.h"

#include "common/protobuf/utility.h"
#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.validate.h"

namespace Nighthawk {
namespace Server {

HttpTestServerDecoderFilterConfig::HttpTestServerDecoderFilterConfig(
    const nighthawk::server::ResponseOptions& proto_config)
    : server_config_(proto_config) {}

HttpTestServerDecoderFilter::HttpTestServerDecoderFilter(
    HttpTestServerDecoderFilterConfigSharedPtr config)
    : config_(config) {}

HttpTestServerDecoderFilter::~HttpTestServerDecoderFilter() {}

void HttpTestServerDecoderFilter::onDestroy() {}

Envoy::Http::FilterHeadersStatus
HttpTestServerDecoderFilter::decodeHeaders(Envoy::Http::HeaderMap& headers, bool) {
  const auto* request_config_header = headers.get(TestServer::HeaderNames::get().TestServerConfig);
  nighthawk::server::ResponseOptions base_config = config_->server_config();

  std::string error_message = "";
  if (request_config_header != nullptr) {
    try {
      nighthawk::server::ResponseOptions json_config;
      Envoy::MessageUtil::loadFromJson(request_config_header->value().c_str(), json_config);
      base_config.MergeFrom(json_config);
      Envoy::MessageUtil::validate(base_config);
    } catch (Envoy::EnvoyException exception) {
      error_message = exception.what();
    }
  }

  if (error_message.empty()) {
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(200), std::string(base_config.response_size(), 'a'),
        [&base_config](Envoy::Http::HeaderMap& direct_response_headers) {
          for (auto header_value_option : base_config.response_headers()) {
            auto header = header_value_option.header();
            auto lower_case_key = Envoy::Http::LowerCaseString(header.key());
            if (!header_value_option.append().value()) {
              direct_response_headers.remove(lower_case_key);
            }
            direct_response_headers.addCopy(lower_case_key, header.value());
          }
        },
        absl::nullopt);
  } else {
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(500),
        fmt::format("test-server didn't understand the request: {}", error_message), nullptr,
        absl::nullopt);
  }
  return Envoy::Http::FilterHeadersStatus::StopIteration;
}

Envoy::Http::FilterDataStatus HttpTestServerDecoderFilter::decodeData(Envoy::Buffer::Instance&,
                                                                      bool) {
  return Envoy::Http::FilterDataStatus::Continue;
}

Envoy::Http::FilterTrailersStatus
HttpTestServerDecoderFilter::decodeTrailers(Envoy::Http::HeaderMap&) {
  return Envoy::Http::FilterTrailersStatus::Continue;
}

void HttpTestServerDecoderFilter::setDecoderFilterCallbacks(
    Envoy::Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Server
} // namespace Nighthawk
