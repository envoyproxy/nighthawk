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

class SslTest : public Test {};

TEST_F(SslTest, FakeAdminCoverage) {
  FakeAdmin admin;
  Envoy::Server::Admin::HandlerCb cb;
  EXPECT_DEATH(admin.addHandler("", "", cb, false, false), "");
  EXPECT_DEATH(admin.removeHandler(""), "");
  EXPECT_DEATH(admin.socket(), "");
  EXPECT_DEATH(admin.getConfigTracker(), "");
  EXPECT_DEATH(admin.startHttpListener("", "", {}, {}, {}), "");
  Envoy::Http::HeaderMapImpl map;
  std::string foo;
  EXPECT_DEATH(admin.request("", "", map, foo), "");
  EXPECT_DEATH(admin.addListenerToHandler(nullptr), "");
}

TEST_F(SslTest, FakeClusterManager) {
  FakeClusterManager manager;
  EXPECT_DEATH(manager.addOrUpdateCluster({}, {}), "");
  EXPECT_DEATH(manager.setInitializedCb([]() {}), "");
  EXPECT_DEATH(manager.clusters(), "");
  EXPECT_DEATH(manager.get(""), "");
  EXPECT_DEATH(manager.httpConnPoolForCluster("", {}, {}, {}), "");
  EXPECT_DEATH(manager.tcpConnPoolForCluster("", {}, {}, {}), "");
  EXPECT_DEATH(manager.tcpConnForCluster("", {}, {}), "");
  EXPECT_DEATH(manager.httpAsyncClientForCluster(""), "");
  EXPECT_DEATH(manager.removeCluster(""), "");
  EXPECT_DEATH(manager.shutdown(), "");
  EXPECT_DEATH(manager.bindConfig(), "");
  EXPECT_DEATH(manager.adsMux(), "");
  EXPECT_DEATH(manager.grpcAsyncClientManager(), "");
  EXPECT_DEATH(manager.localClusterName(), "");
  Envoy::Upstream::MockClusterUpdateCallbacks cb;
  EXPECT_DEATH(manager.addThreadLocalClusterUpdateCallbacks(cb), "");
  EXPECT_DEATH(manager.clusterManagerFactory(), "");
  EXPECT_DEATH(manager.subscriptionFactory(), "");
}

TEST_F(SslTest, MinimalTransportSocketFactoryContextTest) {
  Envoy::Event::MockDispatcher dispatcher;
  Envoy::Runtime::MockRandomGenerator random;
  Envoy::Stats::IsolatedStoreImpl stats;
  Envoy::Api::MockApi api;
  Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl ssl_context_manager(
      api.timeSource());
  Envoy::ProtobufMessage::NullValidationVisitorImpl validation_visitor;
  Envoy::ThreadLocal::MockInstance tls;
  EXPECT_CALL(api, threadFactory()).WillOnce(ReturnRef(Envoy::Thread::threadFactoryForTest()));

  MinimalTransportSocketFactoryContext mtsfc(stats.createScope(""), dispatcher, random, stats, api,
                                             ssl_context_manager, validation_visitor, tls);

  EXPECT_NO_FATAL_FAILURE(mtsfc.admin());
  EXPECT_EQ(&mtsfc.sslContextManager(), &ssl_context_manager);
  EXPECT_NO_FATAL_FAILURE(mtsfc.statsScope());
  EXPECT_NO_FATAL_FAILURE(mtsfc.secretManager());
  EXPECT_NO_FATAL_FAILURE(mtsfc.clusterManager());
  EXPECT_NO_FATAL_FAILURE(mtsfc.localInfo());
  EXPECT_NO_FATAL_FAILURE(mtsfc.dispatcher());
  EXPECT_NO_FATAL_FAILURE(mtsfc.random());
  EXPECT_NO_FATAL_FAILURE(mtsfc.stats());
  Envoy::Init::ManagerImpl manager("test");
  EXPECT_DEATH(mtsfc.setInitManager(manager), "");
  EXPECT_DEATH(mtsfc.initManager(), "");
  EXPECT_NO_FATAL_FAILURE(mtsfc.singletonManager());
  EXPECT_NO_FATAL_FAILURE(mtsfc.threadLocal());
  EXPECT_NO_FATAL_FAILURE(mtsfc.api());
  EXPECT_NO_FATAL_FAILURE(mtsfc.messageValidationVisitor());
}

} // namespace Ssl
} // namespace Nighthawk
