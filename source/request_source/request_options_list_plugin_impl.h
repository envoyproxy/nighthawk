// Implementations of RequestSourceConfigFactories that make a OptionsListRequestSource.
#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/request_source/request_source_plugin_config_factory.h"

#include "external/envoy/source/common/common/lock_guard.h"
#include "external/envoy/source/common/common/thread.h"

#include "api/client/options.pb.h"
#include "api/request_source/request_source_plugin.pb.h"

#include "source/common/uri_impl.h"

namespace Nighthawk {

// Sample Request Source for small RequestOptionsLists. Loads a copy of the RequestOptionsList in
// memory and replays them.
// @param total_requests The number of requests the requestGenerator produced by get() will
// generate. 0 means it is unlimited.
// @param header the default header that will be overridden by values taken from the options_list,
// any values not overridden will be used.
// @param options_list This is const because it is not intended to be modified by the request
// source. The RequestGenerator produced by get() will use options from the options_list to
// overwrite values in the default header, and create new requests. if total_requests is greater
// than the length of options_list, it will loop. If the options_list_ is empty, we just return the
// default header. This is not thread safe.
class OptionsListRequestSource : public RequestSource {
public:
  OptionsListRequestSource(
      const uint32_t total_requests, Envoy::Http::RequestHeaderMapPtr header,
      std::unique_ptr<const nighthawk::client::RequestOptionsList> options_list);

  // This get function is not thread safe, because multiple threads calling get simultaneously will
  // result in a collision.
  RequestGenerator get() override;

  // default implementation
  void initOnThread() override;

private:
  Envoy::Http::RequestHeaderMapPtr header_;
  std::unique_ptr<const nighthawk::client::RequestOptionsList> options_list_;
  std::vector<uint32_t> request_count_;
  const uint32_t total_requests_;
};

// Factory that creates a OptionsListRequestSource from a FileBasedOptionsListRequestSourceConfig
// proto. Registered as an Envoy plugin. Implementation of RequestSourceConfigFactory which produces
// a RequestSource that keeps an RequestOptionsList in memory, and loads it with the RequestOptions
// taken from a file. All plugins configuration are specified in the request_source_plugin.proto.
// This class is thread-safe,
// Usage: assume you are passed an appropriate Any type object called config, an Api
// object called api, and a default header called header. auto& config_factory =
//     Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
//         "nighthawk.file-based-request-source-plugin");
// RequestSourcePtr plugin =
//     config_factory.createRequestSourcePlugin(config, std::move(api), std::move(header));
class FileBasedOptionsListRequestSourceFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  // This implementation is thread safe, and uses a locking mechanism to prevent more than one
  // thread reading the file at the same time. This method will error if the file can not be loaded
  // correctly, e.g. the file is too large or could not be found.
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::Api& api,
                                             Envoy::Http::RequestHeaderMapPtr header) override;

private:
  Envoy::Thread::MutexBasicLockable file_lock_;
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(FileBasedOptionsListRequestSourceFactory);

// Factory that creates a OptionsListRequestSource from a InLineOptionsListRequestSourceConfig
// proto. Registered as an Envoy plugin. Implementation of RequestSourceConfigFactory which produces
// a RequestSource that keeps an RequestOptionsList in memory, and loads it with the RequestOptions
// passed to it from the config. All plugins configuration are specified in the
// request_source_plugin.proto.
// This class is thread-safe,
// Usage: assume you are passed an appropriate Any type object called
// config, an Api object called api, and a default header called header. auto& config_factory =
//     Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
//         "nighthawk.in-line-options-list-request-source-plugin");
// RequestSourcePtr plugin =
//     config_factory.createRequestSourcePlugin(config, std::move(api), std::move(header));

class InLineOptionsListRequestSourceFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  // This implementation is thread safe.
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::Api& api,
                                             Envoy::Http::RequestHeaderMapPtr header) override;
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(InLineOptionsListRequestSourceFactory);

} // namespace Nighthawk