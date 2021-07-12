// Test implementations of RequestSourceConfigFactory and RequestSource that perform minimum
// functionality for testing purposes.
#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/request_source/request_source_plugin_config_factory.h"

#include "api/client/options.pb.h"
#include "api/request_source/request_source_plugin.pb.h"

#include "source/common/uri_impl.h"

namespace Nighthawk {

// Stub Request Source implementation for comparison.
class StubRequestSource : public RequestSource {
public:
  StubRequestSource(const nighthawk::request_source::StubPluginConfig& config);
  // The generator function will return a header whose only value is the test_value taken from the
  // config. The function is threadsafe.
  RequestGenerator get() override;

  // default implementation
  void initOnThread() override;

private:
  const double test_value_;
};

// Factory that creates a StubRequestSource from a StubRequestSourcePluginConfig proto.
// Registered as an Envoy plugin.
// Stub implementation of RequestSourceConfigFactory which produces a RequestSource.
// RequestSources are used to get RequestGenerators which generate requests for the benchmark
// client. All plugins configuration are specified in the request_source_plugin.proto This class is
// thread-safe, but it doesn't do anything. Usage: assume you are passed an appropriate Any type
// object called config, an Api object called api, and a default header called header. auto&
// config_factory =
//     Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
//         "nighthawk.stub-request-source-plugin");
// RequestSourcePtr plugin =
//     config_factory.createRequestSourcePlugin(config, std::move(api), std::move(header));

class StubRequestSourcePluginConfigFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  // This implementation is thread safe, but the RequestSource it generates doesn't do much.
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::Api& api,
                                             Envoy::Http::RequestHeaderMapPtr header) override;
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(StubRequestSourcePluginConfigFactory);
} // namespace Nighthawk