#include "common/request_source_plugin_impl.h"
#include "common/request_impl.h"
#include "common/request_source_impl.h"
// #include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/source/common/protobuf/utility.h"
#include "common/protobuf/message_validator_impl.h"

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

RequestSourcePluginPtr
RPCRequestSourceConfigFactory::createRequestSourcePlugin(const Envoy::Protobuf::Message& message) {
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
    : uri_(config.uri()) {
  //      Envoy::MessageUtil::loadFromJson()
  std::ifstream options_file(uri_);
  if (options_file.is_open())
  {
    std::cerr << "Opened file: " + uri_;
  }
  // options_.ParseFromIstream(&options_file);
  std::stringstream file_string_stream;
  file_string_stream << options_file.rdbuf();
  std::string test_string = file_string_stream.str();
  Envoy::MessageUtil util;
  util.loadFromYaml(test_string, options_, Envoy::ProtobufMessage::getStrictValidationVisitor());
  for (const auto& option_header : options_.request_headers()) {
    std::cerr << option_header.header().key() +":"+ option_header.header().value()+"\n";
  }
  options_file.close();
  std::cerr << "\ntest_string" +test_string;
  std::cerr << "\nI was here";
  std::cerr << "header size:" + std::to_string(options_.request_headers_size());
}
RequestGenerator FileBasedRequestSourcePlugin::get() {
  RequestGenerator request_generator = []() {
    // auto returned_request_impl = std::make_unique<RequestImpl>(std::move(options_.request_headers()));
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
    auto returned_request_impl = std::make_unique<RequestImpl>(std::move(header));
    return returned_request_impl;
  };
  return request_generator;
}
void FileBasedRequestSourcePlugin::initOnThread() {
  // options_.set_request_method(::envoy::config::core::v3::RequestMethod::GET);
  // auto request_headers = options_.add_request_headers();
  // request_headers->mutable_header()->set_key("foo");
  // request_headers->mutable_header()->set_value("bar");
  // options_.set_allocated_request_body_size(0);
}

} // namespace Nighthawk