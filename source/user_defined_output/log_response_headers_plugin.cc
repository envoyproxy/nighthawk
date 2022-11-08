#include "log_response_headers_plugin.h"
#include "source/user_defined_output/log_response_headers_plugin.h"

#include "envoy/http/header_map.h"

#include "external/envoy/source/common/http/utility.h"

#include "api/user_defined_output/log_response_headers.pb.h"

namespace Nighthawk {

namespace {

using ::Envoy::Http::HeaderEntry;
using ::Envoy::Http::HeaderMap;
using ::nighthawk::LogResponseHeadersConfig;

} // namespace

void EnvoyHeaderLogger::LogHeader(const Envoy::Http::HeaderEntry& header_entry) {
  ENVOY_LOG(info, "Received Header with name {} and value {}", header_entry.key().getStringView(),
            header_entry.value().getStringView());
}

LogResponseHeadersPlugin::LogResponseHeadersPlugin(LogResponseHeadersConfig config, WorkerMetadata)
    : config_(std::move(config)), header_logger_(std::make_unique<EnvoyHeaderLogger>()) {}

absl::Status LogResponseHeadersPlugin::handleResponseHeaders(
    const Envoy::Http::ResponseHeaderMap& response_headers) {
  if (config_.logging_mode() == LogResponseHeadersConfig::LM_SKIP_200_LEVEL_RESPONSES) {
    const uint64_t response_code = Envoy::Http::Utility::getResponseStatus(response_headers);
    if (response_code >= 200 && response_code < 300) {
      return absl::OkStatus();
    }
  } else if (config_.logging_mode() == LogResponseHeadersConfig::LM_LOG_ALL_RESPONSES) {
  } else {
    return absl::InvalidArgumentError(
        "Invalid configuration for LogResponseHeadersPlugin. Must provide a valid LoggingMode");
  }

  // If there are no named headers to log, log every header.
  if (config_.log_headers_with_name_size() == 0) {
    HeaderMap::ConstIterateCb log_header_callback = [this](const HeaderEntry& header_entry) {
      header_logger_->LogHeader(header_entry);
      return HeaderMap::Iterate::Continue;
    };
    response_headers.iterate(log_header_callback);
  } else {
    // Iterate through the named headers and log them.
    for (const std::string& header_name : config_.log_headers_with_name()) {
      Envoy::Http::LowerCaseString lowercase_header_name(header_name);
      HeaderMap::GetResult get_result = response_headers.get(lowercase_header_name);
      for (uint i = 0; i < get_result.size(); i++) {
        const HeaderEntry* header_entry = get_result[i];
        header_logger_->LogHeader(*header_entry);
      }
    }
  }
  return absl::OkStatus();
}

void LogResponseHeadersPlugin::injectHeaderLogger(std::unique_ptr<HeaderLogger> logger) {
  header_logger_ = std::move(logger);
}

absl::Status LogResponseHeadersPlugin::handleResponseData(const Envoy::Buffer::Instance&) {
  return absl::OkStatus();
}

absl::StatusOr<Envoy::ProtobufWkt::Any> LogResponseHeadersPlugin::getPerWorkerOutput() const {
  nighthawk::LogResponseHeadersOutput output;
  Envoy::ProtobufWkt::Any any;
  any.PackFrom(output);
  return any;
}

std::string LogResponseHeadersPluginFactory::name() const {
  return "nighthawk.log_response_headers_plugin";
}
Envoy::ProtobufTypes::MessagePtr LogResponseHeadersPluginFactory::createEmptyConfigProto() {
  return std::make_unique<LogResponseHeadersConfig>();
}

UserDefinedOutputPluginPtr LogResponseHeadersPluginFactory::createUserDefinedOutputPlugin(
    const Envoy::Protobuf::Message& message, const WorkerMetadata& worker_metadata) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  LogResponseHeadersConfig config;
  Envoy::MessageUtil::unpackTo(any, config);

  return std::make_unique<LogResponseHeadersPlugin>(config, worker_metadata);
}

absl::StatusOr<Envoy::ProtobufWkt::Any>
LogResponseHeadersPluginFactory::AggregateGlobalOutput(absl::Span<const Envoy::ProtobufWkt::Any>) {
  nighthawk::LogResponseHeadersOutput output;
  Envoy::ProtobufWkt::Any any;
  any.PackFrom(output);
  return any;
}

REGISTER_FACTORY(LogResponseHeadersPluginFactory, UserDefinedOutputPluginFactory);

} // namespace Nighthawk
