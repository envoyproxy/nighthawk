
#pragma once

// TODO(oschaaf): certificate validation.

#include "envoy/network/transport_socket.h"

#include "common/secret/secret_manager_impl.h"

#include "server/transport_socket_config_impl.h"

#include "extensions/transport_sockets/tls/context_config_impl.h"
#include "extensions/transport_sockets/tls/context_impl.h"
#include "extensions/transport_sockets/tls/context_manager_impl.h"
#include "extensions/transport_sockets/tls/ssl_socket.h"

namespace Nighthawk {
namespace Ssl {

class MinimalTransportSocketFactoryContext
    : public Envoy::Server::Configuration::TransportSocketFactoryContext {
public:
  MinimalTransportSocketFactoryContext(
      Envoy::Stats::ScopePtr&& stats_scope, Envoy::Event::Dispatcher& dispatcher,
      Envoy::Runtime::RandomGenerator& random, Envoy::Stats::Store& stats, Envoy::Api::Api& api,
      Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl& ssl_context_manager,
      Envoy::ProtobufMessage::ValidationVisitor& validation_visitor)
      : ssl_context_manager_(ssl_context_manager), stats_scope_(std::move(stats_scope)),
        dispatcher_(dispatcher), random_(random), stats_(stats), api_(api),
        validation_visitor_(validation_visitor) {}

  Envoy::Server::Admin& admin() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::Ssl::ContextManager& sslContextManager() override { return ssl_context_manager_; }

  Envoy::Stats::Scope& statsScope() const override { return *stats_scope_; }

  Envoy::Secret::SecretManager& secretManager() override { return secret_manager_; }

  Envoy::Upstream::ClusterManager& clusterManager() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  const Envoy::LocalInfo::LocalInfo& localInfo() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::Event::Dispatcher& dispatcher() override { return dispatcher_; }

  Envoy::Runtime::RandomGenerator& random() override { return random_; }

  Envoy::Stats::Store& stats() override { return stats_; }

  void setInitManager(Envoy::Init::Manager&) override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::Init::Manager* initManager() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::Singleton::Manager& singletonManager() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::ThreadLocal::SlotAllocator& threadLocal() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::Api::Api& api() override { return api_; }

  Envoy::ProtobufMessage::ValidationVisitor& messageValidationVisitor() override {
    return validation_visitor_;
  }

private:
  Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl& ssl_context_manager_;
  Envoy::Stats::ScopePtr stats_scope_;
  Envoy::Secret::SecretManagerImpl secret_manager_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Runtime::RandomGenerator& random_;
  Envoy::Stats::Store& stats_;
  Envoy::Api::Api& api_;
  Envoy::ProtobufMessage::ValidationVisitor& validation_visitor_;
};

} // namespace Ssl
} // namespace Nighthawk