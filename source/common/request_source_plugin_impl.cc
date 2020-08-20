#include "common/request_source_plugin_impl.h"
#include "common/request_impl.h"
#include "common/request_source_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/request_source/request_source_plugin_impl.pb.h"

namespace Nighthawk {

std::string DummyRequestSourceConfigFactory::name() const {
  return "nighthawk.dummy-request-source-plugin";
}

Envoy::ProtobufTypes::MessagePtr DummyRequestSourceConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::request_source::DummyPluginRequestSourceConfig>();
}

RequestSourcePluginPtr DummyRequestSourceConfigFactory::createRequestSourcePlugin(
    const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::DummyPluginRequestSourceConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<DummyRequestSourcePlugin>(config);
}

REGISTER_FACTORY(DummyRequestSourceConfigFactory, RequestSourcePluginConfigFactory);

DummyRequestSourcePlugin::DummyRequestSourcePlugin(
    const nighthawk::request_source::DummyPluginRequestSourceConfig& config)
    : dummy_value_{config.has_dummy_value() ? config.dummy_value().value()
                                            : std::numeric_limits<double>::infinity()} {}
RequestGenerator DummyRequestSourcePlugin::get() {
  RequestGenerator request_generator = []() {
    Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    auto returned_request_impl = std::make_unique<RequestImpl>(std::move(header));
    return returned_request_impl;
  };
  return request_generator;
}
void DummyRequestSourcePlugin::initOnThread() {}


std::string RPCRequestSourceConfigFactory::name() const {
  return "nighthawk.rpc-request-source-plugin";
}

Envoy::ProtobufTypes::MessagePtr RPCRequestSourceConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::request_source::RPCPluginRequestSourceConfig>();
}

RequestSourcePluginPtr RPCRequestSourceConfigFactory::createRequestSourcePlugin(
    const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::RPCPluginRequestSourceConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<RPCRequestSourcePlugin>(config);
}

REGISTER_FACTORY(RPCRequestSourceConfigFactory, RequestSourcePluginConfigFactory);

RPCRequestSourcePlugin::RPCRequestSourcePlugin(
    const nighthawk::request_source::RPCPluginRequestSourceConfig& config)
    : uri_(config.uri()) {}
RequestGenerator RPCRequestSourcePlugin::get() {
  RequestGenerator request_generator = []() {
    Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    auto returned_request_impl = std::make_unique<RequestImpl>(std::move(header));
    return returned_request_impl;
  };
  return request_generator;
}
void RPCRequestSourcePlugin::initOnThread() {}


std::string FileBasedRequestSourceConfigFactory::name() const {
  return "nighthawk.file-based-request-source-plugin";
}

Envoy::ProtobufTypes::MessagePtr FileBasedRequestSourceConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::request_source::FileBasedPluginRequestSourceConfig>();
}

RequestSourcePluginPtr FileBasedRequestSourceConfigFactory::createRequestSourcePlugin(
    const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FileBasedRequestSourcePlugin>(config);
}

REGISTER_FACTORY(FileBasedRequestSourceConfigFactory, RequestSourcePluginConfigFactory);

FileBasedRequestSourcePlugin::FileBasedRequestSourcePlugin(
    const nighthawk::request_source::FileBasedPluginRequestSourceConfig& config)
    : uri_(config.uri()) {}
RequestGenerator FileBasedRequestSourcePlugin::get() {
  RequestGenerator request_generator = []() {
    Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    auto returned_request_impl = std::make_unique<RequestImpl>(std::move(header));
    return returned_request_impl;
  };
  return request_generator;
}
void FileBasedRequestSourcePlugin::initOnThread() {}

} // namespace Nighthawk