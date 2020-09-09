#include "common/request_source_plugin_impl.h"
#include "common/protobuf/message_validator_impl.h"
#include "common/request_impl.h"
#include "common/request_source_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "external/envoy/source/exe/platform_impl.h"

#include "api/client/options.pb.h"

#include "api/request_source/request_source_plugin_impl.pb.h"
#include <fstream>
#include <iostream>
#include <sstream>
namespace Nighthawk {

std::string DummyRequestSourceConfigFactory::name() const {
  return "nighthawk.dummy-request-source-plugin";
}

Envoy::ProtobufTypes::MessagePtr DummyRequestSourceConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::request_source::DummyPluginRequestSourceConfig>();
}

RequestSourcePluginPtr
DummyRequestSourceConfigFactory::createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                                           Envoy::Api::Api& api) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::DummyPluginRequestSourceConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<DummyRequestSourcePlugin>(config, api);
}

REGISTER_FACTORY(DummyRequestSourceConfigFactory, RequestSourcePluginConfigFactory);

DummyRequestSourcePlugin::DummyRequestSourcePlugin(
    const nighthawk::request_source::DummyPluginRequestSourceConfig& config,
    Envoy::Api::Api& api)
    : RequestSourcePlugin{api}, dummy_value_{config.has_dummy_value() ? config.dummy_value().value()
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

RequestSourcePluginPtr
RPCRequestSourceConfigFactory::createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                                         Envoy::Api::Api& api) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::RPCPluginRequestSourceConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<RPCRequestSourcePlugin>(config, api);
}

REGISTER_FACTORY(RPCRequestSourceConfigFactory, RequestSourcePluginConfigFactory);

RPCRequestSourcePlugin::RPCRequestSourcePlugin(
    const nighthawk::request_source::RPCPluginRequestSourceConfig& config,
    Envoy::Api::Api& api)
    : RequestSourcePlugin{api}, uri_(config.uri()) {}
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
    const Envoy::Protobuf::Message& message, Envoy::Api::Api& api) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FileBasedRequestSourcePlugin>(config, api);
}

REGISTER_FACTORY(FileBasedRequestSourceConfigFactory, RequestSourcePluginConfigFactory);

FileBasedRequestSourcePlugin::FileBasedRequestSourcePlugin(
    const nighthawk::request_source::FileBasedPluginRequestSourceConfig& config,
    Envoy::Api::Api& api)
    : RequestSourcePlugin{api}, uri_(config.uri()), file_path_(config.file_path()) {
  Envoy::MessageUtil util;
  util.loadFromFile(file_path_, optionses_, Envoy::ProtobufMessage::getStrictValidationVisitor(), api_, true);
}

RequestGenerator FileBasedRequestSourcePlugin::get() {
  RequestOptionsIterator iterator = optionses_.sub_options().begin();
  request_iterators_.push_back(iterator);
  RequestOptionsIterator* temp = &request_iterators_.back();
  RequestGenerator request_generator = [this, temp]() mutable {
    auto tempValue = *temp;    
    nighthawk::client::RequestOptions request_option = *tempValue;
    *temp = std::next(*temp);
    Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    header->setPath(uri_.path());
    header->setHost(uri_.hostAndPort());
    header->setScheme(uri_.scheme() == "https" ? Envoy::Http::Headers::get().SchemeValues.Https
                                               : Envoy::Http::Headers::get().SchemeValues.Http);
    header->setMethod(envoy::config::core::v3::RequestMethod_Name(request_option.request_method()));
    const uint32_t content_length = request_option.request_body_size().value();
    if (content_length > 0) {
      header->setContentLength(content_length);
    }
    for (const auto& option_header : request_option.request_headers()) {
        auto lower_case_key = Envoy::Http::LowerCaseString(std::string(option_header.header().key()));
        header->remove(lower_case_key);
        header->addCopy(lower_case_key, std::string(option_header.header().value()));
    }
    auto returned_request_impl = std::make_unique<RequestImpl>(std::move(header));
    return returned_request_impl;
  };
  return request_generator;
}

void FileBasedRequestSourcePlugin::initOnThread() {
}

} // namespace Nighthawk