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

// Returns true if these response_headers should be logged, false if skipped.
bool shouldLogResponse(const nighthawk::LogResponseHeadersConfig& config,
                       const Envoy::Http::ResponseHeaderMap& response_headers) {
  if (config.logging_mode() == LogResponseHeadersConfig::LM_SKIP_200_LEVEL_RESPONSES) {
    const uint64_t response_code = Envoy::Http::Utility::getResponseStatus(response_headers);
    if (response_code >= 200 && response_code < 300) {
      return false;
    }
  }
  return true;
}

void logAllHeaders(HeaderLogger* header_logger,
                   const Envoy::Http::ResponseHeaderMap& response_headers) {
  HeaderMap::ConstIterateCb log_header_callback = [header_logger](const HeaderEntry& header_entry) {
    header_logger->LogHeader(header_entry);
    return HeaderMap::Iterate::Continue;
  };
  response_headers.iterate(log_header_callback);
}

void logSpecifiedHeaders(HeaderLogger* header_logger,
                         const nighthawk::LogResponseHeadersConfig& config,
                         const Envoy::Http::ResponseHeaderMap& response_headers) {
  // Iterate through the named headers and log them.
  for (const std::string& header_name : config.log_headers_with_name()) {
    const Envoy::Http::LowerCaseString lowercase_header_name(header_name);
    const HeaderMap::GetResult get_result = response_headers.get(lowercase_header_name);
    for (uint i = 0; i < get_result.size(); i++) {
      const HeaderEntry* header_entry = get_result[i];
      header_logger->LogHeader(*header_entry);
    }
  }
}

absl::Status validateConfig(LogResponseHeadersConfig config) {
  if (config.logging_mode() == LogResponseHeadersConfig::LM_UNKNOWN) {
    return absl::InvalidArgumentError(
        "Invalid configuration for LogResponseHeadersPlugin. Must provide a valid LoggingMode");
  }

  absl::flat_hash_set<Envoy::Http::LowerCaseString> header_names;
  for (const std::string& header_name : config.log_headers_with_name()) {
    if (header_name.empty()) {
      return absl::InvalidArgumentError(
          "Invalid configuration for LogResponseHeadersPlugin. Received empty header");
    }

    const Envoy::Http::LowerCaseString lowercase_header_name(header_name);
    if (header_names.contains(lowercase_header_name)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid configuration for LogResponseHeadersPlugin. Duplicate header ", header_name));
    }
    header_names.insert(lowercase_header_name);
  }

  return absl::OkStatus();
}

Envoy::ProtobufWkt::Any createEmptyOutput() {
  nighthawk::LogResponseHeadersOutput output;
  Envoy::ProtobufWkt::Any any;
  any.PackFrom(output);
  return any;
}

} // namespace

void EnvoyHeaderLogger::LogHeader(const Envoy::Http::HeaderEntry& header_entry) {
  ENVOY_LOG(info, "Received Header with name {} and value {}", header_entry.key().getStringView(),
            header_entry.value().getStringView());
}

LogResponseHeadersPlugin::LogResponseHeadersPlugin(LogResponseHeadersConfig config, WorkerMetadata)
    : config_(std::move(config)), header_logger_(std::make_unique<EnvoyHeaderLogger>()) {}

absl::Status LogResponseHeadersPlugin::handleResponseHeaders(
    const Envoy::Http::ResponseHeaderMap& response_headers) {
  // TODO(dubious90): validate config on plugin creation when possible.
  absl::Status status = validateConfig(config_);
  if (!status.ok()) {
    return status;
  }

  if (shouldLogResponse(config_, response_headers)) {
    if (config_.log_headers_with_name_size() == 0) {
      logAllHeaders(header_logger_.get(), response_headers);
    } else {
      logSpecifiedHeaders(header_logger_.get(), config_, response_headers);
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
  return createEmptyOutput();
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
  return createEmptyOutput();
}

REGISTER_FACTORY(LogResponseHeadersPluginFactory, UserDefinedOutputPluginFactory);

} // namespace Nighthawk
