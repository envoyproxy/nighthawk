// Implementations of RequestSourceConfigFactory and the RequestSources that those factories make.
#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/common/request_source_plugin_config_factory.h"

#include "external/envoy/source/common/common/lock_guard.h"
#include "external/envoy/source/common/common/thread.h"

#include "api/client/options.pb.h"
#include "api/request_source/request_source_plugin.pb.h"

#include "common/uri_impl.h"

namespace Nighthawk {


// Sample Request Source for small RequestOptionsLists. Loads a copy of the RequestOptionsList in
// memory and replays them.
// @param request_max The number of requests the requestGenerator produced by get() will generate. 0
// means it is unlimited.
// @param header the default header that will be overridden by values taken from the options_list,
// any values not overridden will be used.
// @param options_list A copy of the options_list will be loaded in memory. The RequestGenerator
// produced by get() will use options from the options_list to overwrite values in the header, and
// create new requests. if request_max is greater than the length of options_list, it will loop.
// This is not thread safe.
class RequestOptionsListRequestSource : public RequestSource {
public:
  explicit RequestOptionsListRequestSource(
      const uint32_t request_max, Envoy::Http::RequestHeaderMapPtr header,
      std::unique_ptr<nighthawk::client::RequestOptionsList> options_list);
  // This get function is not thread safe, because multiple threads calling get simultaneously will
  // result in a collision as it attempts to update its request_count_.
  RequestGenerator get() override;

  // default implementation
  void initOnThread() override;

private:
  Envoy::Http::RequestHeaderMapPtr header_;
  const std::unique_ptr<nighthawk::client::RequestOptionsList> options_list_;
  std::vector<uint32_t> request_count_;
  const uint32_t request_max_;
};

// Factory that creates a RequestOptionsListRequestSource from a FileBasedPluginConfig proto.
// Registered as an Envoy plugin.
// Implementation of RequestSourceConfigFactory which produces a RequestSource that keeps an
// RequestOptionsList in memory RequestSources are used to get RequestGenerators which generate
// requests for the benchmark client. All plugins configuration are specified in the
// request_source_plugin.proto This class is not thread-safe, because it loads its RequestOptionlist
// in memory from a file when first called. The in memory RequestOptionsList is protected by
// file_lock_. Usage: assume you are passed an appropriate Any type object called config, an Api
// object called api, and a default header called header. auto& config_factory =
//     Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
//         "nighthawk.file-based-request-source-plugin");
// RequestSourcePtr plugin =
//     config_factory.createRequestSourcePlugin(config, std::move(api), std::move(header));
class FileBasedRequestSourcePluginConfigFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;
  // This returns an empty version of the expected FileBasedPluginConfig from
  // request_source_plugin.proto
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  // This is the primary method that is used to get RequestSources.
  // This implementation is not thread safe. Only the first call to createRequestSourcePlugin will
  // load the file from memory and subsequent calls just make a copy of the options_list that was
  // already loaded. The FileBasedRequestSourcePluginConfigFactory will not work with multiple
  // different files for this reason.
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::ApiPtr api,
                                             Envoy::Http::RequestHeaderMapPtr header) override;

private:
  Envoy::Thread::MutexBasicLockable file_lock_;
  nighthawk::client::RequestOptionsList options_list_ ABSL_GUARDED_BY(file_lock_);
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(FileBasedRequestSourcePluginConfigFactory);


}