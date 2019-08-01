#include "common/ssl.h"

namespace Nighthawk {
namespace Ssl {

bool FakeAdmin::addHandler(const std::string&, const std::string&, Envoy::Server::Admin::HandlerCb,
                           bool, bool) {
  return true;
};

bool FakeAdmin::removeHandler(const std::string&) { return true; };

const Envoy::Network::Socket& FakeAdmin::socket() { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };

Envoy::Server::ConfigTracker& FakeAdmin::getConfigTracker() { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };

void FakeAdmin::startHttpListener(const std::string&, const std::string&,
                                  Envoy::Network::Address::InstanceConstSharedPtr,
                                  const Envoy::Network::Socket::OptionsSharedPtr&,
                                  Envoy::Stats::ScopePtr&&){};

Envoy::Http::Code FakeAdmin::request(absl::string_view, absl::string_view, Envoy::Http::HeaderMap&,
                                     std::string&) {
  return Envoy::Http::Code::OK;
};

void FakeAdmin::addListenerToHandler(Envoy::Network::ConnectionHandler*){};

bool FakeClusterManager::addOrUpdateCluster(const envoy::api::v2::Cluster&, const std::string&) {
  return true;
}

void FakeClusterManager::setInitializedCb(std::function<void()>) {}

Envoy::Upstream::ClusterManager::ClusterInfoMap FakeClusterManager::clusters() {
  NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
}

Envoy::Upstream::ThreadLocalCluster* FakeClusterManager::get(absl::string_view) { return nullptr; }

Envoy::Http::ConnectionPool::Instance*

FakeClusterManager::httpConnPoolForCluster(const std::string&, Envoy::Upstream::ResourcePriority,
                                           Envoy::Http::Protocol,
                                           Envoy::Upstream::LoadBalancerContext*) {
  return nullptr;
}

Envoy::Tcp::ConnectionPool::Instance*
FakeClusterManager::tcpConnPoolForCluster(const std::string&, Envoy::Upstream::ResourcePriority,
                                          Envoy::Upstream::LoadBalancerContext*,
                                          Envoy::Network::TransportSocketOptionsSharedPtr) {
  return nullptr;
}

Envoy::Upstream::Host::CreateConnectionData
FakeClusterManager::tcpConnForCluster(const std::string&, Envoy::Upstream::LoadBalancerContext*,
                                      Envoy::Network::TransportSocketOptionsSharedPtr) {
  NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
}

Envoy::Http::AsyncClient& FakeClusterManager::httpAsyncClientForCluster(const std::string&) {
  NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
}

bool FakeClusterManager::removeCluster(const std::string&) { return true; }

void FakeClusterManager::shutdown() {}

const envoy::api::v2::core::BindConfig& FakeClusterManager::bindConfig() const {
  NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
}

Envoy::Config::GrpcMux& FakeClusterManager::adsMux() { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

Envoy::Grpc::AsyncClientManager& FakeClusterManager::grpcAsyncClientManager() {
  NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
}

const std::string& FakeClusterManager::localClusterName() const { return foo_string_; }

Envoy::Upstream::ClusterUpdateCallbacksHandlePtr
FakeClusterManager::addThreadLocalClusterUpdateCallbacks(Envoy::Upstream::ClusterUpdateCallbacks&) {
  return nullptr;
}

Envoy::Upstream::ClusterManagerFactory& FakeClusterManager::clusterManagerFactory() {
  NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
}

Envoy::Config::SubscriptionFactory& FakeClusterManager::subscriptionFactory() {
  NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
}

std::size_t FakeClusterManager::warmingClusterCount() const { return 0u; }

MinimalTransportSocketFactoryContext::MinimalTransportSocketFactoryContext(
    Envoy::Stats::ScopePtr&& stats_scope, Envoy::Event::Dispatcher& dispatcher,
    Envoy::Runtime::RandomGenerator& random, Envoy::Stats::Store& stats, Envoy::Api::Api& api,
    Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl& ssl_context_manager,
    Envoy::ProtobufMessage::ValidationVisitor& validation_visitor,
    Envoy::ThreadLocal::Instance& tls)
    : ssl_context_manager_(ssl_context_manager), stats_scope_(std::move(stats_scope)),
      secret_manager_(config_tracker_), dispatcher_(dispatcher), random_(random), stats_(stats),
      api_(api), validation_visitor_(validation_visitor),
      local_info_({},
                  Envoy::Network::Utility::getLocalAddress(Envoy::Network::Address::IpVersion::v4),
                  "nighthawk_service_zone", "nighthawk_service_cluster", "nighthawk_service_node"),
      manager_(api_.threadFactory()), tls_(tls) {}

Envoy::Server::Admin& MinimalTransportSocketFactoryContext::admin() { return admin_; }

Envoy::Ssl::ContextManager& MinimalTransportSocketFactoryContext::sslContextManager() {
  return ssl_context_manager_;
}

Envoy::Stats::Scope& MinimalTransportSocketFactoryContext::statsScope() const {
  return *stats_scope_;
}

Envoy::Secret::SecretManager& MinimalTransportSocketFactoryContext::secretManager() {
  return secret_manager_;
}

Envoy::Upstream::ClusterManager& MinimalTransportSocketFactoryContext::clusterManager() {
  return cluster_manager_;
}

const Envoy::LocalInfo::LocalInfo& MinimalTransportSocketFactoryContext::localInfo() {
  return local_info_;
}

Envoy::Event::Dispatcher& MinimalTransportSocketFactoryContext::dispatcher() { return dispatcher_; }

Envoy::Runtime::RandomGenerator& MinimalTransportSocketFactoryContext::random() { return random_; }

Envoy::Stats::Store& MinimalTransportSocketFactoryContext::stats() { return stats_; }

void MinimalTransportSocketFactoryContext::setInitManager(Envoy::Init::Manager&) {}

Envoy::Init::Manager* MinimalTransportSocketFactoryContext::initManager() { return nullptr; }

Envoy::Singleton::Manager& MinimalTransportSocketFactoryContext::singletonManager() {
  return manager_;
}

Envoy::ThreadLocal::SlotAllocator& MinimalTransportSocketFactoryContext::threadLocal() {
  return tls_;
}

Envoy::Api::Api& MinimalTransportSocketFactoryContext::api() { return api_; }

Envoy::ProtobufMessage::ValidationVisitor&
MinimalTransportSocketFactoryContext::messageValidationVisitor() {
  return validation_visitor_;
}

} // namespace Ssl
} // namespace Nighthawk