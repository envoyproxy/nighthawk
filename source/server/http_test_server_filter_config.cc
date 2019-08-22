#include <string>

#include "external/envoy/source/common/config/json_utility.h"

#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"

#include "server/http_test_server_filter.h"

#include "envoy/registry/registry.h"

namespace Nighthawk {
namespace Server {
namespace Configuration {

class HttpTestServerDecoderFilterConfig
    : public Envoy::Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Envoy::Http::FilterFactoryCb
  createFilterFactory(const Envoy::Json::Object&, const std::string&,
                      Envoy::Server::Configuration::FactoryContext&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  Envoy::Http::FilterFactoryCb
  createFilterFactoryFromProto(const Envoy::Protobuf::Message& proto_config, const std::string&,
                               Envoy::Server::Configuration::FactoryContext& context) override {

    return createFilter(
        Envoy::MessageUtil::downcastAndValidate<const nighthawk::server::ResponseOptions&>(
            proto_config),
        context);
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return Envoy::ProtobufTypes::MessagePtr{new nighthawk::server::ResponseOptions()};
  }

  std::string name() override { return "test-server"; }

private:
  Envoy::Http::FilterFactoryCb createFilter(const nighthawk::server::ResponseOptions& proto_config,
                                            Envoy::Server::Configuration::FactoryContext&) {
    Nighthawk::Server::HttpTestServerDecoderFilterConfigSharedPtr config =
        std::make_shared<Nighthawk::Server::HttpTestServerDecoderFilterConfig>(
            Nighthawk::Server::HttpTestServerDecoderFilterConfig(proto_config));

    return [config](Envoy::Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto* filter = new Nighthawk::Server::HttpTestServerDecoderFilter(config);
      callbacks.addStreamDecoderFilter(Envoy::Http::StreamDecoderFilterSharedPtr{filter});
    };
  }
};

static Envoy::Registry::RegisterFactory<HttpTestServerDecoderFilterConfig,
                                        Envoy::Server::Configuration::NamedHttpFilterConfigFactory>
    register_;
} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
