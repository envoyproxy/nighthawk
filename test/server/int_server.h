/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "common/api/api_impl.h"
#include "common/grpc/common.h"
#include "common/http/codec_client.h"
#include "common/network/listen_socket_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "test/test_common/test_time.h"
#include "test/test_common/utility.h"

#include "common/filesystem/filesystem_impl.h"

namespace Mixer {
namespace Integration {

enum class ServerCloseReason {
  REMOTE_CLOSE, // Peer closed or connection was reset after it was established.
  LOCAL_CLOSE   // This process decided to close the connection.
};

enum class ServerCallbackResult {
  CONTINUE, // Leave the connection open
  CLOSE     // Close the connection.
};

class ServerStream {
public:
  ServerStream();

  virtual ~ServerStream();

  ServerStream(ServerStream&&) = default;
  ServerStream& operator=(ServerStream&&) = default;

  /**
   * Send a HTTP header-only response and close the stream.
   *
   * @param response_headers the response headers
   * @param delay delay in msec before sending the response.  if 0 send immediately
   */
  virtual void
  sendResponseHeaders(const Envoy::Http::HeaderMap& response_headers,
                      const std::chrono::milliseconds delay = std::chrono::milliseconds(0)) PURE;

  /**
   * Send a gRPC response and close the stream
   *
   * @param status The gRPC status (carried in the HTTP response trailer)
   * @param response The gRPC response (carried in the HTTP response body)
   * @param delay delay in msec before sending the response.  if 0 send immediately
   */
  virtual void
  sendGrpcResponse(Envoy::Grpc::Status::GrpcStatus status, const Envoy::Protobuf::Message& response,
                   const std::chrono::milliseconds delay = std::chrono::milliseconds(0)) PURE;

  // private:
  //  ServerStream(const ServerStream&) = delete;
  //  void operator=(const ServerStream&) = delete;
};

using ServerStreamPtr = std::unique_ptr<ServerStream>;
using ServerStreamSharedPtr = std::shared_ptr<ServerStream>;

class ServerConnection;

// NB: references passed to any of these callbacks are owned by the caller and must not be used
// after the callback returns -- except for the request headers which may be moved into the caller.
using ServerAcceptCallback = std::function<ServerCallbackResult(ServerConnection&)>;
using ServerCloseCallback = std::function<void(ServerConnection&, ServerCloseReason)>;
// TODO support sending delayed responses
using ServerRequestCallback =
    std::function<void(ServerConnection&, ServerStream&, Envoy::Http::HeaderMapPtr)>;

class ServerConnection : public Envoy::Network::ReadFilter,
                         public Envoy::Network::ConnectionCallbacks,
                         public Envoy::Http::ServerConnectionCallbacks,
                         Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
public:
  ServerConnection(absl::string_view name, uint32_t id, ServerRequestCallback request_callback,
                   ServerCloseCallback close_callback,
                   Envoy::Network::Connection& network_connection,
                   Envoy::Event::Dispatcher& dispatcher, Envoy::Http::CodecClient::Type http_type,
                   Envoy::Stats::Scope& scope);

  ~ServerConnection() override;

  // ServerConnection(ServerConnection&&) = default;
  // ServerConnection& operator=(ServerConnection&&) = default;

  absl::string_view name() const;

  uint32_t id() const;

  Envoy::Network::Connection& networkConnection();
  const Envoy::Network::Connection& networkConnection() const;

  Envoy::Http::ServerConnection& httpConnection();
  const Envoy::Http::ServerConnection& httpConnection() const;

  Envoy::Event::Dispatcher& dispatcher();

  /**
   * For internal use
   */
  void removeStream(uint32_t stream_id);

  //
  // Envoy::Network::ReadFilter
  //

  Envoy::Network::FilterStatus onData(Envoy::Buffer::Instance& data, bool end_stream) override;

  Envoy::Network::FilterStatus onNewConnection() override;

  void initializeReadFilterCallbacks(Envoy::Network::ReadFilterCallbacks&) override;

  //
  // Envoy::Http::ConnectionCallbacks
  //

  void onGoAway() override;

  //
  // Envoy::Http::ServerConnectionCallbacks
  //

  Envoy::Http::StreamDecoder& newStream(Envoy::Http::StreamEncoder& stream_encoder,
                                        bool is_internally_created = false) override;

  //
  // Envoy::Network::ConnectionCallbacks
  //

  void onEvent(Envoy::Network::ConnectionEvent event) override;

  void onAboveWriteBufferHighWatermark() override;

  void onBelowWriteBufferLowWatermark() override;

private:
  // ServerConnection(const ServerConnection&) = delete;
  // ServerConnection& operator=(const ServerConnection&) = delete;

  std::string name_;
  uint32_t id_;
  Envoy::Network::Connection& network_connection_;
  Envoy::Http::ServerConnectionPtr http_connection_;
  Envoy::Event::Dispatcher& dispatcher_;
  ServerRequestCallback request_callback_;
  ServerCloseCallback close_callback_;

  std::mutex streams_lock_;
  std::unordered_map<uint32_t, ServerStreamPtr> streams_;
  uint32_t stream_counter_{0U};
};

using ServerConnectionPtr = std::unique_ptr<ServerConnection>;
using ServerConnectionSharedPtr = std::shared_ptr<ServerConnection>;

class ServerFilterChain : public Envoy::Network::FilterChain {
public:
  ServerFilterChain(Envoy::Network::TransportSocketFactory& transport_socket_factory);

  ~ServerFilterChain() override;

  // ServerFilterChain(ServerFilterChain&&) = default;
  // ServerFilterChain& operator=(ServerFilterChain&&) = default;

  //
  // Envoy::Network::FilterChain
  //

  const Envoy::Network::TransportSocketFactory& transportSocketFactory() const override;

  const std::vector<Envoy::Network::FilterFactoryCb>& networkFilterFactories() const override;

private:
  // ServerFilterChain(const ServerFilterChain&) = delete;
  // ServerFilterChain& operator=(const ServerFilterChain&) = delete;

  Envoy::Network::TransportSocketFactory& transport_socket_factory_;
  std::vector<Envoy::Network::FilterFactoryCb> network_filter_factories_;
};

/**
 * A convenience class for creating a listening socket bound to localhost
 */
class LocalListenSocket : public Envoy::Network::TcpListenSocket {
public:
  /**
   * Create a listening socket bound to localhost.
   *
   * @param ip_version v4 or v6.  v4 by default.
   * @param port the port.  If 0, let the kernel allocate an avaiable ephemeral port.  0 by default.
   * @param options socket options.  nullptr by default
   * @param bind_to_port if true immediately bind to the port, allocating one if necessary.  true by
   * default.
   */
  LocalListenSocket(
      Envoy::Network::Address::IpVersion ip_version = Envoy::Network::Address::IpVersion::v4,
      uint16_t port = 0, const Envoy::Network::Socket::OptionsSharedPtr& options = nullptr,
      bool bind_to_port = true);
  // LocalListenSocket(LocalListenSocket&&) = default;
  // LocalListenSocket& operator=(LocalListenSocket&&) = default;

  // private:
  // LocalListenSocket(const LocalListenSocket&) = delete;
  // void operator=(const LocalListenSocket&) = delete;
};

/**
 * A convenience class for passing callbacks to a Server.  If no callbacks are provided, default
 * callbacks that track some simple metrics will be used.   If callbacks are provided, they will be
 * wrapped with callbacks that maintain the same simple set of metrics.
 */
class ServerCallbackHelper {
public:
  ServerCallbackHelper(ServerRequestCallback request_callback = nullptr,
                       ServerAcceptCallback accept_callback = nullptr,
                       ServerCloseCallback close_callback = nullptr);

  virtual ~ServerCallbackHelper();
  // ServerCallbackHelper(ServerCallbackHelper&&) = default;
  // ServerCallbackHelper& operator=(ServerCallbackHelper&&) = default;

  uint32_t connectionsAccepted() const;
  uint32_t requestsReceived() const;
  uint32_t localCloses() const;
  uint32_t remoteCloses() const;
  ServerAcceptCallback acceptCallback() const;
  ServerRequestCallback requestCallback() const;
  ServerCloseCallback closeCallback() const;

  /*
   * Wait until the server has accepted n connections and seen them closed (due to error or client
   * close)
   */
  void wait(uint32_t connections);

  /*
   * Wait until the server has seen a close for every connection it has accepted.
   */
  void wait();

  ServerCallbackHelper(const ServerCallbackHelper&) = delete;
  void operator=(const ServerCallbackHelper&) = delete;

private:
  ServerAcceptCallback accept_callback_;
  ServerRequestCallback request_callback_;
  ServerCloseCallback close_callback_;

  std::atomic<uint32_t> accepts_{0};
  std::atomic<uint32_t> requests_received_{0};
  std::atomic<uint32_t> local_closes_{0};
  std::atomic<uint32_t> remote_closes_{0};
  std::mutex mutex_;
  std::condition_variable condvar_;
};

using ServerCallbackHelperPtr = std::unique_ptr<ServerCallbackHelper>;
using ServerCallbackHelperSharedPtr = std::shared_ptr<ServerCallbackHelper>;

class Server : public Envoy::Network::FilterChainManager,
               public Envoy::Network::FilterChainFactory,
               public Envoy::Network::ListenerConfig,
               Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
public:
  // TODO make use of Network::Socket::OptionsSharedPtr
  Server(absl::string_view name, Envoy::Network::Socket& listening_socket,
         Envoy::Network::TransportSocketFactory& transport_socket_factory,
         Envoy::Http::CodecClient::Type http_type);

  ~Server() override;
  // Server(Server&&) = default;
  // Server& operator=(Server&&) = default;

  void start(ServerAcceptCallback accept_callback, ServerRequestCallback request_callback,
             ServerCloseCallback close_callback);

  void start(ServerCallbackHelper& helper);

  void stop();

  void stopAcceptingConnections();

  void startAcceptingConnections();

  const Envoy::Stats::Store& statsStore() const;

  // TODO does this affect socket recv buffer size?  Only for new connections?
  void setPerConnectionBufferLimitBytes(uint32_t limit);

  //
  // Envoy::Network::ListenerConfig
  //

  Envoy::Network::FilterChainManager& filterChainManager() override;

  Envoy::Network::FilterChainFactory& filterChainFactory() override;

  Envoy::Network::Socket& socket() override;

  const Envoy::Network::Socket& socket() const override;

  bool bindToPort() override;

  bool handOffRestoredDestinationConnections() const override;

  // TODO does this affect socket recv buffer size?  Only for new connections?
  uint32_t perConnectionBufferLimitBytes() const override;

  std::chrono::milliseconds listenerFiltersTimeout() const override;

  Envoy::Stats::Scope& listenerScope() override;

  uint64_t listenerTag() const override;

  const std::string& name() const override;

  //
  // Envoy::Network::FilterChainManager
  //

  const Envoy::Network::FilterChain*
  findFilterChain(const Envoy::Network::ConnectionSocket&) const override;

  //
  // Envoy::Network::FilterChainFactory
  //

  bool createNetworkFilterChain(Envoy::Network::Connection& network_connection,
                                const std::vector<Envoy::Network::FilterFactoryCb>&) override;

  bool createListenerFilterChain(Envoy::Network::ListenerFilterManager&) override;

  Server(const Server&) = delete;
  void operator=(const Server&) = delete;

private:
  std::string name_;
  Envoy::Stats::IsolatedStoreImpl stats_;
  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Api::Impl api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::Network::ConnectionHandlerPtr connection_handler_;
  Envoy::Thread::ThreadPtr thread_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  std::atomic<bool> is_running{false};

  ServerAcceptCallback accept_callback_{nullptr};
  ServerRequestCallback request_callback_{nullptr};
  ServerCloseCallback close_callback_{nullptr};

  //
  // Envoy::Network::ListenerConfig
  //

  Envoy::Network::Socket& listening_socket_;
  std::atomic<uint32_t> connection_buffer_limit_bytes_{0U};

  //
  // Envoy::Network::FilterChainManager
  //

  ServerFilterChain server_filter_chain_;

  //
  // Envoy::Network::FilterChainFactory
  //

  Envoy::Http::CodecClient::Type http_type_;
  std::atomic<uint32_t> connection_counter_{0U};
};

using ServerPtr = std::unique_ptr<Server>;
using ServerSharedPtr = std::shared_ptr<Server>;

class ClusterHelper {
public:
  /*template <typename... Args>
  ClusterHelper(Args &&... args) : servers_(std::forward<Args>(args)...){};*/

  ClusterHelper(std::initializer_list<ServerCallbackHelper*> server_callbacks);

  virtual ~ClusterHelper();

  const std::vector<ServerCallbackHelperPtr>& servers() const;
  std::vector<ServerCallbackHelperPtr>& servers();

  uint32_t connectionsAccepted() const;
  uint32_t requestsReceived() const;
  uint32_t localCloses() const;
  uint32_t remoteCloses() const;

  void wait();

  ClusterHelper(const ClusterHelper&) = delete;
  void operator=(const ClusterHelper&) = delete;

private:
  std::vector<ServerCallbackHelperPtr> server_callback_helpers_;
};

} // namespace Integration
} // namespace Mixer