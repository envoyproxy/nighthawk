#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "nighthawk/common/request_source.h"

namespace Nighthawk {

// A factory that must be implemented for each RequestSourcePlugin. It instantiates the specific
// RequestSourcePlugin class after unpacking the plugin-specific config proto.
class RequestSourcePluginConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~RequestSourcePluginConfigFactory() override = default;
  // All request source plugins will be in this category.
  std::string category() const override { return "nighthawk.request_source_plugin"; }

  // Instantiates the specific RequestSourcePlugin class. Casts |message| to Any, unpacks it to the
  // plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
  //
  // @param typed_config Any typed_config proto taken from the TypedExtensionConfig. This should be
  // a type listed in request_source_plugin_config.proto
  //
  // @param api Api parameter that contains timesystem, filesystem, and threadfactory.
  //
  // @param header RequestHeaderMapPtr parameter that acts as a template header for the
  // requestSource to modify when generating requests.
  //
  // @return RequestSourcePtr Pointer to the new instance of RequestSource.
  //
  // @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
  // plugin.
  virtual RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& typed_config,
                                                     Envoy::Api::Api& api,
                                                     Envoy::Http::RequestHeaderMapPtr header) PURE;
};

} // namespace Nighthawk
