#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"
#include "external/envoy/source/common/common/statusor.h"
#include "nighthawk/common/request_source.h"


namespace Nighthawk {

/**
 * An interface for different RequestSource plugins.
 * Uses a plugin-specific config proto.
 */
class RequestSourcePlugin : RequestSource {
public:
  virtual ~RequestSourcePlugin() = default;

};


/**
 * A factory that must be implemented for each RequestSourcePlugin. It instantiates the specific
 * RequestSourcePlugin class after unpacking the plugin-specific config proto.
 */
class RequestSourcePluginConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~RequestSourcePluginConfigFactory() override = default;
  std::string category() const override { return "nighthawk.request_source_plugin"; }
  /**
   * Instantiates the specific RequestSourcePlugin class. Casts |message| to Any, unpacks it to the
   * plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
   *
   * @param message Any typed_config proto taken from the TypedExtensionConfig.
   *
   * @return RequestSourcePtr Pointer to the new plugin instance.
   *
   * @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
   * plugin.
   */
  virtual RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message) PURE;
};

} // namespace Nighthawk