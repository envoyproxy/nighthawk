#include <chrono>

#include "common/http/header_map_impl.h"
#include "common/ssl.h"

#include "test/mocks/api/mocks.h"
#include "test/mocks/event/mocks.h"
#include "test/mocks/filesystem/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/mocks.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Ssl {

constexpr char message[] = "panic: not implemented";

class SslTest : public Test {};

TEST_F(SslTest, FakeAdminCoverage) {
  FakeAdmin admin;
  Envoy::Server::Admin::HandlerCb cb;
  EXPECT_DEATH(admin.addHandler("", "", cb, false, false), message);
  EXPECT_DEATH(admin.removeHandler(""), message);
  EXPECT_DEATH(admin.socket(), message);
  EXPECT_DEATH(admin.getConfigTracker(), message);
  EXPECT_DEATH(admin.startHttpListener("", "", {}, {}, {}), message);
  Envoy::Http::HeaderMapImpl map;
  std::string foo;
  EXPECT_DEATH(admin.request("", "", map, foo), message);
  EXPECT_DEATH(admin.addListenerToHandler(nullptr), message);
}

TEST_F(SslTest, FakeClusterManager) {
  FakeClusterManager manager;
  EXPECT_DEATH(manager.addOrUpdateCluster({}, {}), message);
  EXPECT_DEATH(manager.setInitializedCb([]() {}), message);
  EXPECT_DEATH(manager.clusters(), message);
  EXPECT_DEATH(manager.get(""), message);
  EXPECT_DEATH(manager.httpConnPoolForCluster("", {}, {}, {}), message);
  EXPECT_DEATH(manager.tcpConnPoolForCluster("", {}, {}, {}), message);
  EXPECT_DEATH(manager.tcpConnForCluster("", {}, {}), message);
  EXPECT_DEATH(manager.httpAsyncClientForCluster(""), message);
  EXPECT_DEATH(manager.removeCluster(""), message);
  EXPECT_DEATH(manager.shutdown(), message);
  EXPECT_DEATH(manager.bindConfig(), message);
  EXPECT_DEATH(manager.adsMux(), message);
  EXPECT_DEATH(manager.grpcAsyncClientManager(), message);
  EXPECT_DEATH(manager.localClusterName(), message);
  Envoy::Upstream::MockClusterUpdateCallbacks cb;
  EXPECT_DEATH(manager.addThreadLocalClusterUpdateCallbacks(cb), message);
  EXPECT_DEATH(manager.clusterManagerFactory(), message);
  EXPECT_DEATH(manager.subscriptionFactory(), message);
}

TEST_F(SslTest, MinimalTransportSocketFactoryContextTest) {
  Envoy::Stats::ScopePtr stats_scope;
  Envoy::Event::MockDispatcher dispatcher;
  Envoy::Runtime::MockRandomGenerator random;
  Envoy::Stats::MockStore stats;
  Envoy::Api::MockApi api;
  Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl ssl_context_manager(
      api.timeSource());
  Envoy::ProtobufMessage::NullValidationVisitorImpl validation_visitor;
  Envoy::ThreadLocal::MockInstance tls;
  EXPECT_CALL(api, threadFactory()).WillOnce(ReturnRef(Envoy::Thread::threadFactoryForTest()));

  MinimalTransportSocketFactoryContext mtsc(std::move(stats_scope), dispatcher, random, stats, api,
                                            ssl_context_manager, validation_visitor, tls);

  EXPECT_NO_FATAL_FAILURE(mtsc.admin());
  EXPECT_EQ(&mtsc.sslContextManager(), &ssl_context_manager);
  EXPECT_NO_FATAL_FAILURE(mtsc.statsScope());
  EXPECT_NO_FATAL_FAILURE(mtsc.secretManager());
  EXPECT_NO_FATAL_FAILURE(mtsc.clusterManager());
  EXPECT_NO_FATAL_FAILURE(mtsc.localInfo());
  EXPECT_NO_FATAL_FAILURE(mtsc.dispatcher());
  EXPECT_NO_FATAL_FAILURE(mtsc.random());
  EXPECT_NO_FATAL_FAILURE(mtsc.stats());
  Envoy::Init::ManagerImpl manager("test");
  EXPECT_DEATH(mtsc.setInitManager(manager), message);
  EXPECT_DEATH(mtsc.initManager(), message);
  EXPECT_NO_FATAL_FAILURE(mtsc.singletonManager());
  EXPECT_NO_FATAL_FAILURE(mtsc.threadLocal());
  EXPECT_NO_FATAL_FAILURE(mtsc.api());
  EXPECT_NO_FATAL_FAILURE(mtsc.messageValidationVisitor());
}

} // namespace Ssl
} // namespace Nighthawk
