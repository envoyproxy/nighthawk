#include <string>

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"

#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"

#include "source/server/configuration.h"
#include "source/server/http_time_tracking_filter.h"

namespace Nighthawk {
namespace Server {
namespace Configuration {

class HttpTimeTrackingFilterConfig
    : public Envoy::Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Envoy::Http::FilterFactoryCb
  createFilterFactoryFromProto(const Envoy::Protobuf::Message& proto_config, const std::string&,
                               Envoy::Server::Configuration::FactoryContext& context) override {
    Envoy::ProtobufMessage::ValidationVisitor& validation_visitor =
        Envoy::ProtobufMessage::getStrictValidationVisitor();
    const nighthawk::server::ResponseOptions& response_options =
        Envoy::MessageUtil::downcastAndValidate<const nighthawk::server::ResponseOptions&>(
            proto_config, validation_visitor);
    validateResponseOptions(response_options);
    return createFilter(response_options, context);
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return Envoy::ProtobufTypes::MessagePtr{new nighthawk::server::ResponseOptions()};
  }

  std::string name() const override { return "time-tracking"; }

private:
  Envoy::Http::FilterFactoryCb createFilter(const nighthawk::server::ResponseOptions& proto_config,
                                            Envoy::Server::Configuration::FactoryContext&) {
    Nighthawk::Server::HttpTimeTrackingFilterConfigSharedPtr config =
        std::make_shared<Nighthawk::Server::HttpTimeTrackingFilterConfig>(
            Nighthawk::Server::HttpTimeTrackingFilterConfig(proto_config));

    return [config](Envoy::Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto* filter = new Nighthawk::Server::HttpTimeTrackingFilter(config);
      callbacks.addStreamFilter(Envoy::Http::StreamFilterSharedPtr{filter});
    };
  }
};

static Envoy::Registry::RegisterFactory<HttpTimeTrackingFilterConfig,
                                        Envoy::Server::Configuration::NamedHttpFilterConfigFactory>
    register_;
} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
