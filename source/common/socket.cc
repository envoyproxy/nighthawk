#include "common/socket.h"

#include "external/envoy/source/common/config/utility.h"

namespace Nighthawk {

class SocketConfigFactoryImpl : public UpstreamSocketConfigFactory {};

Envoy::ProtobufTypes::MessagePtr UpstreamSocketConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::transport_socket::TransportSocket>();
}

Envoy::Network::TransportSocketFactoryPtr UpstreamSocketConfigFactory::createTransportSocketFactory(
    const Envoy::Protobuf::Message& message,
    Envoy::Server::Configuration::TransportSocketFactoryContext& context) {
  const auto& outer_config =
      Envoy::MessageUtil::downcastAndValidate<const nighthawk::transport_socket::TransportSocket&>(
          message, context.messageValidationVisitor());
  auto& inner_config_factory = Envoy::Config::Utility::getAndCheckFactory<
      Envoy::Server::Configuration::UpstreamTransportSocketConfigFactory>(
      outer_config.transport_socket());
  Envoy::ProtobufTypes::MessagePtr inner_factory_config =
      Envoy::Config::Utility::translateToFactoryConfig(outer_config.transport_socket(),
                                                       context.messageValidationVisitor(),
                                                       inner_config_factory);
  auto inner_transport_factory =
      inner_config_factory.createTransportSocketFactory(*inner_factory_config, context);
  return std::make_unique<SocketFactory>(outer_config, SocketConfigFactoryImpl(),
                                         context.scope().createScope("upstream_socket."),
                                         std::move(inner_transport_factory));
}

SocketFactory::SocketFactory(const nighthawk::transport_socket::TransportSocket&,
                             SocketConfigFactory&&, Envoy::Stats::ScopePtr&& scope,
                             Envoy::Network::TransportSocketFactoryPtr&& transport_socket_factory)
    : scope_(std::move(scope)), transport_socket_factory_(std::move(transport_socket_factory)) {}

// Network::TransportSocketFactory
Envoy::Network::TransportSocketPtr SocketFactory::createTransportSocket(
    Envoy::Network::TransportSocketOptionsSharedPtr options) const {
  return std::make_unique<Socket>(*scope_,
                                  transport_socket_factory_->createTransportSocket(options));
}

bool SocketFactory::implementsSecureTransport() const {
  return transport_socket_factory_->implementsSecureTransport();
}

Socket::Socket(Envoy::Stats::Scope& scope, Envoy::Network::TransportSocketPtr&& transport_socket)
    : scope_(scope), transport_socket_(std::move(transport_socket)),
      socket_stats_({ALL_SOCKET_STATS(POOL_COUNTER(scope_))}) {}

void Socket::setTransportSocketCallbacks(Envoy::Network::TransportSocketCallbacks& callbacks) {
  transport_socket_->setTransportSocketCallbacks(callbacks);
}

std::string Socket::protocol() const { return transport_socket_->protocol(); }
absl::string_view Socket::failureReason() const { return transport_socket_->failureReason(); }

bool Socket::canFlushClose() { return transport_socket_->canFlushClose(); }

void Socket::closeSocket(Envoy::Network::ConnectionEvent event) {
  socket_stats_.closes_.inc();
  transport_socket_->closeSocket(event);
}

Envoy::Network::IoResult Socket::doRead(Envoy::Buffer::Instance& buffer) {
  Envoy::Network::IoResult result = transport_socket_->doRead(buffer);
  socket_stats_.reads_.inc();
  if (result.bytes_processed_) {
    socket_stats_.read_bytes_.add(result.bytes_processed_);
  }
  return result;
}

Envoy::Network::IoResult Socket::doWrite(Envoy::Buffer::Instance& buffer, bool end_stream) {
  Envoy::Network::IoResult result = transport_socket_->doWrite(buffer, end_stream);
  socket_stats_.writes_.inc();
  if (result.bytes_processed_) {
    socket_stats_.write_bytes_.add(result.bytes_processed_);
  }
  return result;
}

void Socket::onConnected() {
  socket_stats_.connects_.inc();
  transport_socket_->onConnected();
}

Envoy::Ssl::ConnectionInfoConstSharedPtr Socket::ssl() const { return transport_socket_->ssl(); }

REGISTER_FACTORY(UpstreamSocketConfigFactory,
                 Envoy::Server::Configuration::UpstreamTransportSocketConfigFactory);

} // namespace Nighthawk
