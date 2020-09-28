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

// Sample Request Source implementation for comparison.
class DummyRequestSource : public RequestSource {
public:
  explicit DummyRequestSource(
      const nighthawk::request_source::StubPluginConfig& config);
  RequestGenerator get() override;

  //default implementation
  void initOnThread() override;
};

/**
 * Factory that creates a DummyRequestSource from a DummyRequestSourcePluginConfig proto.
 * Registered as an Envoy plugin.
 */
class DummyRequestSourcePluginConfigFactory : public virtual RequestSourcePluginConfigFactory {
public:
  
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::ApiPtr api,
                                             Envoy::Http::RequestHeaderMapPtr header) override;
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(DummyRequestSourcePluginConfigFactory);

using RequestOptionsIterator =
    Envoy::ProtobufWkt::internal::RepeatedPtrIterator<const nighthawk::client::RequestOptions>;

/**
 * Sample Request Source for small files. Loads the file in and replays the request specifications
 * from the file. Each worker will keep the file contents in memory. It will provide num_requests
 * number of requests, looping as required. 0 requests means infinite requests.
 */
class RequestOptionsListRequestSource : public RequestSource {
public:
  explicit RequestOptionsListRequestSource(
      const uint32_t request_max, Envoy::Http::RequestHeaderMapPtr header,
      std::unique_ptr<nighthawk::client::RequestOptionsList> options_list);
  RequestGenerator get() override;

  //default implementation
  void initOnThread() override;

private:
  Envoy::Http::RequestHeaderMapPtr header_;
  const std::unique_ptr<nighthawk::client::RequestOptionsList> options_list_;
  std::vector<RequestOptionsIterator> request_iterators_;
  std::vector<uint32_t> request_count_;
  const uint32_t request_max_;
};

/**
 * Registered as an Envoy plugin.
 */
class FileBasedRequestSourcePluginConfigFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::ApiPtr api,
                                             Envoy::Http::RequestHeaderMapPtr header) override;

private:
  Envoy::Thread::MutexBasicLockable file_lock_;
  nighthawk::client::RequestOptionsList options_list_ ABSL_GUARDED_BY(file_lock_);
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(FileBasedRequestSourcePluginConfigFactory);

} // namespace Nighthawk
