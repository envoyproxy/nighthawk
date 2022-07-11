#include <string>

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"

#include "api/server/dynamic_delay.pb.h"
#include "api/server/dynamic_delay.pb.validate.h"

#include "source/server/configuration.h"
#include "source/server/http_dynamic_delay_filter.h"

namespace Nighthawk {
namespace Server {
namespace Configuration {
namespace {

class HttpDynamicDelayDecoderFilterConfigFactory
    : public Envoy::Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Envoy::Http::FilterFactoryCb
  createFilterFactoryFromProto(const Envoy::Protobuf::Message& proto_config, const std::string&,
                               Envoy::Server::Configuration::FactoryContext& context) override {

    auto& validation_visitor = Envoy::ProtobufMessage::getStrictValidationVisitor();
    const nighthawk::server::DynamicDelayConfiguration& dynamic_delay_configuration =
        Envoy::MessageUtil::downcastAndValidate<
            const nighthawk::server::DynamicDelayConfiguration&>(proto_config, validation_visitor);
    return createFilter(dynamic_delay_configuration, context);
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return Envoy::ProtobufTypes::MessagePtr{new nighthawk::server::DynamicDelayConfiguration()};
  }

  std::string name() const override { return "dynamic-delay"; }

private:
  Envoy::Http::FilterFactoryCb
  createFilter(const nighthawk::server::DynamicDelayConfiguration& proto_config,
               Envoy::Server::Configuration::FactoryContext& context) {
    Nighthawk::Server::HttpDynamicDelayDecoderFilterConfigSharedPtr config =
        std::make_shared<Nighthawk::Server::HttpDynamicDelayDecoderFilterConfig>(
            Nighthawk::Server::HttpDynamicDelayDecoderFilterConfig(
                proto_config, context.runtime(), "" /*stats_prefix*/, context.scope(),
                context.timeSource()));

    return [config](Envoy::Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto* filter = new Nighthawk::Server::HttpDynamicDelayDecoderFilter(config);
      callbacks.addStreamDecoderFilter(Envoy::Http::StreamDecoderFilterSharedPtr{filter});
    };
  }
};

} // namespace

static Envoy::Registry::RegisterFactory<HttpDynamicDelayDecoderFilterConfigFactory,
                                        Envoy::Server::Configuration::NamedHttpFilterConfigFactory>
    register_;
} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
