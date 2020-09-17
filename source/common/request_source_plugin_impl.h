#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/common/request_source_plugin.h"

#include "external/envoy/source/common/common/lock_guard.h"
#include "external/envoy/source/common/common/thread.h"

#include "api/client/options.pb.h"
#include "api/request_source/request_source_plugin.pb.h"

#include "common/uri_impl.h"

namespace Nighthawk {

/**
 * Sample Request Source implementation for comparison.
 */
class DummyRequestSourcePlugin : public RequestSource {
public:
  explicit DummyRequestSourcePlugin(
      const nighthawk::request_source::DummyPluginRequestSourceConfig& config);
  RequestGenerator get() override;
  /**
   * Will be called on an intialized and running worker thread, before commencing actual work.
   * Can be used to prepare the request source implementation (opening any connection or files
   * needed, for example).
   */
  void initOnThread() override;

private:
  const double dummy_value_;
};

/**
 * Factory that creates a DummyRequestSourcePlugin from a DummyRequestSourcePluginConfig proto.
 * Registered as an Envoy plugin.
 */
class DummyRequestSourceConfigFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::Api& api) override;
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(DummyRequestSourceConfigFactory);

using RequestOptionsIterator =
    Envoy::ProtobufWkt::internal::RepeatedPtrIterator<const nighthawk::client::RequestOptions>;

/**
 * Sample Request Source for small files. Loads the file in and replays the request specifications
 * from the file. Each worker will keep the file contents in memory. It will provide num_requests
 * number of requests, looping as required. 0 requests means infinite requests.
 */
class FileBasedRequestSourcePlugin : public RequestSource {
public:
  explicit FileBasedRequestSourcePlugin(
      const nighthawk::request_source::FileBasedPluginRequestSourceConfig& config,
      std::unique_ptr<nighthawk::client::RequestOptionsList> options_list);
  RequestGenerator get() override;
  /**
   * Will be called on an intialized and running worker thread, before commencing actual work.
   * Can be used to prepare the request source implementation (opening any connection or files
   * needed, for example).
   */
  void initOnThread() override;

private:
  const Nighthawk::UriImpl uri_;
  const std::string file_path_;
  const std::unique_ptr<nighthawk::client::RequestOptionsList> options_list_;
  std::vector<RequestOptionsIterator> request_iterators_;
  std::vector<uint32_t> request_count_;
  const uint32_t request_max_;
};
/**
 * Registered as an Envoy plugin.
 */
class FileBasedRequestSourceConfigFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                             Envoy::Api::Api& api) override;

private:
  Envoy::Thread::MutexBasicLockable file_lock_;
  nighthawk::client::RequestOptionsList options_list_ ABSL_GUARDED_BY(file_lock_);
  ;
};

// This factory will be activated through RequestSourceFactory in factories.h
DECLARE_FACTORY(FileBasedRequestSourceConfigFactory);

} // namespace Nighthawk
