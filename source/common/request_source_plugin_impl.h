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
class DummyRequestSourceConfigFactory : public virtual RequestSourcePluginConfigFactory
{
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePluginPtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message) override;
};

// This factory is activated through ???.
DECLARE_FACTORY(DummyRequestSourceConfigFactory);

/**
 */
class RPCRequestSourcePlugin : public RequestSourcePlugin {
public:
  explicit RPCRequestSourcePlugin(
      const nighthawk::request_source::RPCPluginRequestSourceConfig& config);
  RequestGenerator get() override;
  /**
   * Will be called on an intialized and running worker thread, before commencing actual work.
   * Can be used to prepare the request source implementation (opening any connection or files
   * needed, for example).
   */
  void initOnThread() override;

private:
  const std::string uri_;
};

/**
 * Registered as an Envoy plugin.
 */
class RPCRequestSourceConfigFactory : public virtual RequestSourcePluginConfigFactory
{
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePluginPtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message) override;
};

// This factory is activated through ???.
DECLARE_FACTORY(RPCRequestSourceConfigFactory);

/**
 */
class FileBasedRequestSourcePlugin : public RequestSourcePlugin {
public:
  explicit FileBasedRequestSourcePlugin(
      const nighthawk::request_source::FileBasedPluginRequestSourceConfig& config);
  RequestGenerator get() override;
  /**
   * Will be called on an intialized and running worker thread, before commencing actual work.
   * Can be used to prepare the request source implementation (opening any connection or files
   * needed, for example).
   */
  void initOnThread() override;

private:
  const std::string uri_;
};

/**
 * Registered as an Envoy plugin.
 */
class FileBasedRequestSourceConfigFactory : public virtual RequestSourcePluginConfigFactory
{
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  RequestSourcePluginPtr createRequestSourcePlugin(const Envoy::Protobuf::Message& message) override;
};

// This factory is activated through ???.
DECLARE_FACTORY(FileBasedRequestSourceConfigFactory);

}