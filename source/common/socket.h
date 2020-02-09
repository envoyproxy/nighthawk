#include "envoy/registry/registry.h"
#include "envoy/server/transport_socket_config.h"

#include "api/client/socket.pb.h"
#include "api/client/socket.pb.validate.h"

namespace Nighthawk {

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
                SocketConfigFactory&& config_factory, Envoy::Server::Admin& admin,
                Envoy::Singleton::Manager& singleton_manager,
                Envoy::ThreadLocal::SlotAllocator& tls,
                Envoy::Event::Dispatcher& main_thread_dispatcher,
                Envoy::Network::TransportSocketFactoryPtr&& transport_socket_factory);

  // Network::TransportSocketFactory
  Envoy::Network::TransportSocketPtr
  createTransportSocket(Envoy::Network::TransportSocketOptionsSharedPtr options) const override;
  bool implementsSecureTransport() const override;

private:
  Envoy::Network::TransportSocketFactoryPtr transport_socket_factory_;
};

class Socket : public Envoy::Network::TransportSocket {
public:
  Socket(Envoy::Network::TransportSocketPtr&& transport_socket);

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
  Envoy::Network::TransportSocketPtr transport_socket_;
};

} // namespace Nighthawk