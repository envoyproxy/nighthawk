#pragma once

#include "envoy/http/header_map.h"
#include "envoy/registry/registry.h"

#include "nighthawk/user_defined_output/user_defined_output_plugin.h"

#include "external/envoy/source/common/common/statusor.h"

#include "api/user_defined_output/log_response_headers.pb.h"

namespace Nighthawk {

// An abstract class used by LogResponseHeadersPlugin for logging headers.
class HeaderLogger {
public:
  virtual void LogHeader(Envoy::Http::HeaderEntry header_entry) PURE;
};

class EnvoyHeaderLogger : public HeaderLogger,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
  void LogHeader(Envoy::Http::HeaderEntry header_entry) override;
};

/**
 * UserDefinedOutputPlugin for logging response headers received. Stores no internal data. Can be
 * configured to log specific headers only, or only in specific situations.
 */
class LogResponseHeadersPlugin : public UserDefinedOutputPlugin {
public:
  /**
   * Initializes the User Defined Output Plugin.
   *
   * @param config LogResponseHeadersConfig proto for setting when this plugin will log which
   * headers
   * @param worker_metadata Information from the calling worker.
   */
  LogResponseHeadersPlugin(nighthawk::LogResponseHeadersConfig config,
                           WorkerMetadata worker_metadata, HeaderLogger logger);

  /**
   * Logs headers according to the provided configuration.
   */
  absl::Status handleResponseHeaders(const Envoy::Http::ResponseHeaderMap& headers) override;

  /**
   * Performs no actions.
   */
  absl::Status handleResponseData(const Envoy::Buffer::Instance& response_data) override;

  /**
   * Returns empty LogHeadersOutput.
   */
  absl::StatusOr<Envoy::ProtobufWkt::Any> getPerWorkerOutput() const override;

private:
  nighthawk::LogResponseHeadersConfig config_;
  std::unique_ptr<HeaderLogger> header_logger_;
};

/**
 * Factory that creates a LogResponseHeadersPlugin plugin from a LogResponseHeadersConfig
 * proto. Registered as an Envoy plugin.
 */
class LogResponseHeadersPluginFactory : public UserDefinedOutputPluginFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  UserDefinedOutputPluginPtr
  createUserDefinedOutputPlugin(const Envoy::Protobuf::Message& config_any,
                                const WorkerMetadata& worker_metadata) override;

  absl::StatusOr<Envoy::ProtobufWkt::Any>
  AggregateGlobalOutput(absl::Span<const Envoy::ProtobufWkt::Any> per_worker_outputs) override;
};

DECLARE_FACTORY(LogResponseHeadersPluginFactory);

} // namespace Nighthawk
