#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/client/options.h"

namespace Nighthawk {

// A factory that must be implemented for each RateLimiterPlugin. It instantiates the specific
// RateLimiterPlugin class after unpacking the plugin-specific config proto.
class RateLimiterPluginConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~RateLimiterPluginConfigFactory() override = default;
  // All rate limiter plugins will be in this category.
  std::string category() const override { return "nighthawk.rate_limiter_plugin"; }

  // Instantiates the specific RateLimiterPlugin class. Casts |typed_config| to Any, unpacks it to the
  // plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
  //
  // @param typed_config Taken from TypedExtensionConfig. This should be a type
  // listed in a rate limiter proto file.
  //
  // @param api Api parameter that contains timesystem, filesystem, and threadfactory.
  //
  // @param time_source TimeSource parameter used by many rate limiters to track time.
  //
  // @param options Client Options parameter used for command line error checking.
  //
  // @return RateLimiterPtr Pointer to the new instance of RateLimiter.
  //
  // @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
  // plugin.
  virtual RateLimiterPtr createRateLimiterPlugin(const Envoy::Protobuf::Message& typed_config,
                                                 Envoy::Api::Api& api,
                                                 Envoy::TimeSource& time_source,
                                                 const Client::Options& options) PURE;
};

} // namespace Nighthawk