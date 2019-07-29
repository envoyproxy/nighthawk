
#pragma once

// TODO(oschaaf): certificate validation.

#include "envoy/network/transport_socket.h"

#include "common/local_info/local_info_impl.h"
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

class FakeAdmin : public Envoy::Server::Admin {
public:
  bool addHandler(const std::string&, const std::string&, Envoy::Server::Admin::HandlerCb, bool,
                  bool) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  };
  bool removeHandler(const std::string&) override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  const Envoy::Network::Socket& socket() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  Envoy::Server::ConfigTracker& getConfigTracker() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };
  void startHttpListener(const std::string&, const std::string&,
                         Envoy::Network::Address::InstanceConstSharedPtr,
                         const Envoy::Network::Socket::OptionsSharedPtr&,
                         Envoy::Stats::ScopePtr&&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  };
  Envoy::Http::Code request(absl::string_view, absl::string_view, Envoy::Http::HeaderMap&,
                            std::string&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  };
  void addListenerToHandler(Envoy::Network::ConnectionHandler*) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  };
};

class FakeClusterManager : public Envoy::Upstream::ClusterManager {
public:
  bool addOrUpdateCluster(const envoy::api::v2::Cluster&, const std::string&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  void setInitializedCb(std::function<void()>) override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  Envoy::Upstream::ClusterManager::ClusterInfoMap clusters() override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Upstream::ThreadLocalCluster* get(absl::string_view) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Http::ConnectionPool::Instance*
  httpConnPoolForCluster(const std::string&, Envoy::Upstream::ResourcePriority,
                         Envoy::Http::Protocol, Envoy::Upstream::LoadBalancerContext*) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Tcp::ConnectionPool::Instance*
  tcpConnPoolForCluster(const std::string&, Envoy::Upstream::ResourcePriority,
                        Envoy::Upstream::LoadBalancerContext*,
                        Envoy::Network::TransportSocketOptionsSharedPtr) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Upstream::Host::CreateConnectionData
  tcpConnForCluster(const std::string&, Envoy::Upstream::LoadBalancerContext*,
                    Envoy::Network::TransportSocketOptionsSharedPtr) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Http::AsyncClient& httpAsyncClientForCluster(const std::string&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  bool removeCluster(const std::string&) override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  void shutdown() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  const envoy::api::v2::core::BindConfig& bindConfig() const override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Config::GrpcMux& adsMux() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  Envoy::Grpc::AsyncClientManager& grpcAsyncClientManager() override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  const std::string& localClusterName() const override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  Envoy::Upstream::ClusterUpdateCallbacksHandlePtr
  addThreadLocalClusterUpdateCallbacks(Envoy::Upstream::ClusterUpdateCallbacks&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Upstream::ClusterManagerFactory& clusterManagerFactory() override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  Envoy::Config::SubscriptionFactory& subscriptionFactory() override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  std::size_t warmingClusterCount() const override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
};

class MinimalTransportSocketFactoryContext
    : public Envoy::Server::Configuration::TransportSocketFactoryContext {
public:
  MinimalTransportSocketFactoryContext(
      Envoy::Stats::ScopePtr&& stats_scope, Envoy::Event::Dispatcher& dispatcher,
      Envoy::Runtime::RandomGenerator& random, Envoy::Stats::Store& stats, Envoy::Api::Api& api,
      Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl& ssl_context_manager,
      Envoy::ProtobufMessage::ValidationVisitor& validation_visitor,
      Envoy::ThreadLocal::Instance& tls)
      : ssl_context_manager_(ssl_context_manager), stats_scope_(std::move(stats_scope)),
        secret_manager_(config_tracker_), dispatcher_(dispatcher), random_(random), stats_(stats),
        api_(api), validation_visitor_(validation_visitor),
        local_info_(
            {}, Envoy::Network::Utility::getLocalAddress(Envoy::Network::Address::IpVersion::v4),
            "nighthawk_service_zone", "nighthawk_service_cluster", "nighthawk_service_node"),
        manager_(api_.threadFactory()), tls_(tls) {}

  Envoy::Server::Admin& admin() override { return admin_; }

  Envoy::Ssl::ContextManager& sslContextManager() override { return ssl_context_manager_; }

  Envoy::Stats::Scope& statsScope() const override { return *stats_scope_; }

  Envoy::Secret::SecretManager& secretManager() override { return secret_manager_; }

  Envoy::Upstream::ClusterManager& clusterManager() override { return cluster_manager_; }

  const Envoy::LocalInfo::LocalInfo& localInfo() override { return local_info_; }

  Envoy::Event::Dispatcher& dispatcher() override { return dispatcher_; }

  Envoy::Runtime::RandomGenerator& random() override { return random_; }

  Envoy::Stats::Store& stats() override { return stats_; }

  void setInitManager(Envoy::Init::Manager&) override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::Init::Manager* initManager() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  Envoy::Singleton::Manager& singletonManager() override { return manager_; }

  Envoy::ThreadLocal::SlotAllocator& threadLocal() override { return tls_; }

  Envoy::Api::Api& api() override { return api_; }

  Envoy::ProtobufMessage::ValidationVisitor& messageValidationVisitor() override {
    return validation_visitor_;
  }

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