#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"
#include "external/envoy/source/exe/platform_impl.h"

#include "api/client/options.pb.h"

#include "common/request_impl.h"
#include "common/request_source_impl.h"

#include "request_source/request_source_plugin_impl.h"

namespace Nighthawk {

std::string DummyRequestSourcePluginConfigFactory::name() const {
  return "nighthawk.stub-request-source-plugin";
}

Envoy::ProtobufTypes::MessagePtr DummyRequestSourcePluginConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::request_source::StubPluginConfig>();
}

RequestSourcePtr DummyRequestSourcePluginConfigFactory::createRequestSourcePlugin(
    const Envoy::Protobuf::Message& message, Envoy::Api::ApiPtr, Envoy::Http::RequestHeaderMapPtr) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::StubPluginConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<DummyRequestSource>(config);
}

REGISTER_FACTORY(DummyRequestSourcePluginConfigFactory, RequestSourcePluginConfigFactory);

DummyRequestSource::DummyRequestSource(const nighthawk::request_source::StubPluginConfig& config) : test_value_{config.test_value().value()} {}
RequestGenerator DummyRequestSource::get() {

  RequestGenerator request_generator = [this] () {
    Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    header->setCopy(Envoy::Http::LowerCaseString("test_value_"), std::to_string(test_value_));
    auto returned_request_impl = std::make_unique<RequestImpl>(std::move(header));
    return returned_request_impl;
  };
  return request_generator;
}

void DummyRequestSource::initOnThread() {}

} // namespace Nighthawk