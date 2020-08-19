#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/common/request_source_plugin.h"

#include "api/request_source/request_source_plugin_impl.pb.h"

namespace Nighthawk {

/**
 */
class DummyRequestSourcePlugin : public RequestSourcePlugin {
public:
  explicit DummyRequestSourcePlugin(
      const nighthawk::request_source_plugin::DummyPluginRequestSourceConfig& config);

private:
  const double dummy_value_;
};

/**
 * Factory that creates a DummyRequestSourcePlugin from a DummyRequestSourcePluginConfig proto.
 * Registered as an Envoy plugin.
 */
class DummyRequestSourceConfigFactory : public virtual RequestSourcePluginConfigFactory
{
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message) override;
};

// This factory is activated through ???.
DECLARE_FACTORY(DummyRequestSourceConfigFactory);
}