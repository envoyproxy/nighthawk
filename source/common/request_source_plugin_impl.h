#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/common/request_source_plugin.h"

#include "api/client/options.pb.h"
#include "api/request_source/request_source_plugin_impl.pb.h"

#include "common/uri_impl.h"

namespace Nighthawk {

/**
 */
class DummyRequestSourcePlugin : public RequestSourcePlugin {
public:
  explicit DummyRequestSourcePlugin(
      const nighthawk::request_source::DummyPluginRequestSourceConfig& config,
      Envoy::Api::Api& api);
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
  RequestSourcePluginPtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                                   Envoy::Api::Api& api) override;
};

// This factory is activated through ???.
DECLARE_FACTORY(DummyRequestSourceConfigFactory);

using RequestOptionsIterator =
    google::protobuf::internal::RepeatedPtrIterator<const nighthawk::client::RequestOptions>;

/**
 */
class FileBasedRequestSourcePlugin : public RequestSourcePlugin {
public:
  explicit FileBasedRequestSourcePlugin(
      const nighthawk::request_source::FileBasedPluginRequestSourceConfig& config,
      Envoy::Api::Api& api);
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
  nighthawk::client::RequestOptions options_;
  nighthawk::client::RequestOptionses optionses_;
  std::vector<RequestOptionsIterator> request_iterators_;
};
/**
 * Registered as an Envoy plugin.
 */
class FileBasedRequestSourceConfigFactory : public virtual RequestSourcePluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePluginPtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                                   Envoy::Api::Api& api) override;
};

// This factory is activated through ???.
DECLARE_FACTORY(FileBasedRequestSourceConfigFactory);

} // namespace Nighthawk