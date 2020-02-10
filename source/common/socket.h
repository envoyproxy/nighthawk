#include "envoy/registry/registry.h"
#include "envoy/server/transport_socket_config.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/store.h"

#include "api/client/socket.pb.h"
#include "api/client/socket.pb.validate.h"

namespace Nighthawk {

using namespace Envoy; // We need this because of macro expectations.

#define ALL_SOCKET_STATS(COUNTER)                                                                  \
  COUNTER(closes)                                                                                  \
  COUNTER(connects)                                                                                \
  COUNTER(write_bytes)                                                                             \
  COUNTER(writes)                                                                                  \
  COUNTER(read_bytes)                                                                              \
  COUNTER(reads)

struct SocketStats {
  ALL_SOCKET_STATS(GENERATE_COUNTER_STRUCT)
};

class SocketConfigFactory
    : public virtual Envoy::Server::Configuration::TransportSocketConfigFactory {
public:
  ~SocketConfigFactory() override = default;
  std::string name() const override { return "NighthawkSocket"; }
};

class UpstreamSocketConfigFactory
    : public Envoy::Server::Configuration::UpstreamTransportSocketConfigFactory,
      public SocketConfigFactory {
public:
  Envoy::Network::TransportSocketFactoryPtr createTransportSocketFactory(
      const Envoy::Protobuf::Message& config,
      Envoy::Server::Configuration::TransportSocketFactoryContext& context) override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
};

class SocketFactory : public Envoy::Network::TransportSocketFactory {
public:
  SocketFactory(const nighthawk::transport_socket::TransportSocket& proto_config,
                SocketConfigFactory&& config_factory, Envoy::Stats::ScopePtr&& scope,
                Envoy::Network::TransportSocketFactoryPtr&& transport_socket_factory);

  // Network::TransportSocketFactory
  Envoy::Network::TransportSocketPtr
  createTransportSocket(Envoy::Network::TransportSocketOptionsSharedPtr options) const override;
  bool implementsSecureTransport() const override;

private:
  Envoy::Stats::ScopePtr scope_;
  Envoy::Network::TransportSocketFactoryPtr transport_socket_factory_;
};

class Socket : public Envoy::Network::TransportSocket {
public:
  Socket(Envoy::Stats::Scope& scope, Envoy::Network::TransportSocketPtr&& transport_socket);

  // Network::TransportSocket
  void setTransportSocketCallbacks(Envoy::Network::TransportSocketCallbacks& callbacks) override;
  std::string protocol() const override;
  absl::string_view failureReason() const override;
  bool canFlushClose() override;
  void closeSocket(Envoy::Network::ConnectionEvent event) override;
  Envoy::Network::IoResult doRead(Envoy::Buffer::Instance& buffer) override;
  Envoy::Network::IoResult doWrite(Envoy::Buffer::Instance& buffer, bool end_stream) override;
  void onConnected() override;
  Envoy::Ssl::ConnectionInfoConstSharedPtr ssl() const override;

private:
  Envoy::Stats::Scope& scope_;
  Envoy::Network::TransportSocketPtr transport_socket_;
  SocketStats socket_stats_;
};

} // namespace Nighthawk