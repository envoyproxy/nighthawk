
#pragma once

// TODO(oschaaf): certificate validation.

#include "envoy/network/transport_socket.h"

#include "common/local_info/local_info_impl.h"
#include "common/network/utility.h"
#include "common/secret/secret_manager_impl.h"
#include "common/singleton/manager_impl.h"
#include "common/thread_local/thread_local_impl.h"

#include "server/http/config_tracker_impl.h"
#include "server/transport_socket_config_impl.h"

#include "extensions/transport_sockets/tls/context_config_impl.h"
#include "extensions/transport_sockets/tls/context_impl.h"
#include "extensions/transport_sockets/tls/context_manager_impl.h"
#include "extensions/transport_sockets/tls/ssl_socket.h"

namespace Nighthawk {
namespace Ssl {

// Shim class that we aneed a concrete implementations for, but
// which isn't actually used.
class FakeAdmin : public Envoy::Server::Admin {
public:
  bool addHandler(const std::string&, const std::string&, Envoy::Server::Admin::HandlerCb, bool,
                  bool) override;
  bool removeHandler(const std::string&) override;
  const Envoy::Network::Socket& socket() override;
  Envoy::Server::ConfigTracker& getConfigTracker() override;
  void startHttpListener(const std::string&, const std::string&,
                         Envoy::Network::Address::InstanceConstSharedPtr,
                         const Envoy::Network::Socket::OptionsSharedPtr&,
                         Envoy::Stats::ScopePtr&&) override;
  Envoy::Http::Code request(absl::string_view, absl::string_view, Envoy::Http::HeaderMap&,
                            std::string&) override;
  void addListenerToHandler(Envoy::Network::ConnectionHandler*) override;

private:
  Envoy::Server::ConfigTrackerImpl config_tracker_;
};

// Shim class that we aneed a concrete implementations for, but
// which isn't actually used.
class FakeClusterManager : public Envoy::Upstream::ClusterManager {
public:
  bool addOrUpdateCluster(const envoy::api::v2::Cluster&, const std::string&) override;
  void setInitializedCb(std::function<void()>) override;
  Envoy::Upstream::ClusterManager::ClusterInfoMap clusters() override;
  Envoy::Upstream::ThreadLocalCluster* get(absl::string_view) override;
  Envoy::Http::ConnectionPool::Instance*
  httpConnPoolForCluster(const std::string&, Envoy::Upstream::ResourcePriority,
                         Envoy::Http::Protocol, Envoy::Upstream::LoadBalancerContext*) override;
  Envoy::Tcp::ConnectionPool::Instance*
  tcpConnPoolForCluster(const std::string&, Envoy::Upstream::ResourcePriority,
                        Envoy::Upstream::LoadBalancerContext*,
                        Envoy::Network::TransportSocketOptionsSharedPtr) override;
  Envoy::Upstream::Host::CreateConnectionData
  tcpConnForCluster(const std::string&, Envoy::Upstream::LoadBalancerContext*,
                    Envoy::Network::TransportSocketOptionsSharedPtr) override;
  Envoy::Http::AsyncClient& httpAsyncClientForCluster(const std::string&) override;
  bool removeCluster(const std::string&) override;
  void shutdown() override;
  const envoy::api::v2::core::BindConfig& bindConfig() const override;
  Envoy::Config::GrpcMux& adsMux() override;
  Envoy::Grpc::AsyncClientManager& grpcAsyncClientManager() override;
  const std::string& localClusterName() const override;
  Envoy::Upstream::ClusterUpdateCallbacksHandlePtr
  addThreadLocalClusterUpdateCallbacks(Envoy::Upstream::ClusterUpdateCallbacks&) override;
  Envoy::Upstream::ClusterManagerFactory& clusterManagerFactory() override;
  Envoy::Config::SubscriptionFactory& subscriptionFactory() override;
  std::size_t warmingClusterCount() const override;

private:
  std::string foo_string_;
};

class MinimalTransportSocketFactoryContext
    : public Envoy::Server::Configuration::TransportSocketFactoryContext {
public:
  MinimalTransportSocketFactoryContext(
      Envoy::Stats::ScopePtr&& stats_scope, Envoy::Event::Dispatcher& dispatcher,
      Envoy::Runtime::RandomGenerator& random, Envoy::Stats::Store& stats, Envoy::Api::Api& api,
      Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl& ssl_context_manager,
      Envoy::ProtobufMessage::ValidationVisitor& validation_visitor,
      Envoy::ThreadLocal::Instance& tls);

  Envoy::Server::Admin& admin() override;
  Envoy::Ssl::ContextManager& sslContextManager() override;
  Envoy::Stats::Scope& statsScope() const override;
  Envoy::Secret::SecretManager& secretManager() override;
  Envoy::Upstream::ClusterManager& clusterManager() override;
  const Envoy::LocalInfo::LocalInfo& localInfo() override;
  Envoy::Event::Dispatcher& dispatcher() override;
  Envoy::Runtime::RandomGenerator& random() override;
  Envoy::Stats::Store& stats() override;
  void setInitManager(Envoy::Init::Manager&) override;
  Envoy::Init::Manager* initManager() override;
  Envoy::Singleton::Manager& singletonManager() override;
  Envoy::ThreadLocal::SlotAllocator& threadLocal() override;
  Envoy::Api::Api& api() override;
  Envoy::ProtobufMessage::ValidationVisitor& messageValidationVisitor() override;

private:
  Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl& ssl_context_manager_;
  Envoy::Stats::ScopePtr stats_scope_;
  Envoy::Server::ConfigTrackerImpl config_tracker_;
  Envoy::Secret::SecretManagerImpl secret_manager_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Runtime::RandomGenerator& random_;
  Envoy::Stats::Store& stats_;
  Envoy::Api::Api& api_;
  Envoy::ProtobufMessage::ValidationVisitor& validation_visitor_;
  FakeAdmin admin_;
  FakeClusterManager cluster_manager_;
  Envoy::LocalInfo::LocalInfoImpl local_info_;
  Envoy::Singleton::ManagerImpl manager_;
  Envoy::ThreadLocal::Instance& tls_;
};

} // namespace Ssl
} // namespace Nighthawk