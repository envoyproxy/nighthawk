#include "test/request_source/stub_plugin_impl.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"
#include "external/envoy/source/exe/platform_impl.h"

#include "api/client/options.pb.h"

#include "source/common/request_impl.h"
#include "source/common/request_source_impl.h"

namespace Nighthawk {

std::string StubRequestSourcePluginConfigFactory::name() const {
  return "nighthawk.stub-request-source-plugin";
}

Envoy::ProtobufTypes::MessagePtr StubRequestSourcePluginConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::request_source::StubPluginConfig>();
}

RequestSourcePtr StubRequestSourcePluginConfigFactory::createRequestSourcePlugin(
    const Envoy::Protobuf::Message& message, Envoy::Api::Api&, Envoy::Http::RequestHeaderMapPtr) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::StubPluginConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<StubRequestSource>(config);
}

REGISTER_FACTORY(StubRequestSourcePluginConfigFactory, RequestSourcePluginConfigFactory);

StubRequestSource::StubRequestSource(const nighthawk::request_source::StubPluginConfig& config)
    : test_value_{config.has_test_value() ? config.test_value().value() : 0} {}
RequestGenerator StubRequestSource::get() {

  RequestGenerator request_generator = [this]() {
    Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    header->setCopy(Envoy::Http::LowerCaseString("test_value"), std::to_string(test_value_));
    auto returned_request_impl = std::make_unique<RequestImpl>(std::move(header));
    return returned_request_impl;
  };
  return request_generator;
}

void StubRequestSource::initOnThread() {}

} // namespace Nighthawk