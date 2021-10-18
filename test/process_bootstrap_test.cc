#include <string>
#include <vector>

#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/test/mocks/event/mocks.h"
#include "external/envoy/test/mocks/network/mocks.h"
#include "external/envoy/test/test_common/status_utility.h"
#include "external/envoy/test/test_common/utility.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.validate.h"
#include "external/envoy_api/envoy/config/core/v3/base.pb.h"
#include "external/envoy_api/envoy/extensions/transport_sockets/tls/v3/tls.pb.h"

#include "source/client/options_impl.h"
#include "source/client/process_bootstrap.h"
#include "source/common/uri_impl.h"

#include "test/client/utility.h"
#include "test/test_common/proto_matchers.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::envoy::config::bootstrap::v3::Bootstrap;
using ::Envoy::StatusHelpers::StatusIs;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

// Parses text into Bootstrap.
absl::StatusOr<Bootstrap> parseBootstrapFromText(const std::string& bootstrap_text) {
  Bootstrap bootstrap;
  if (!Envoy::Protobuf::TextFormat::ParseFromString(bootstrap_text, &bootstrap)) {
    return absl::InvalidArgumentError(
        fmt::format("cannot parse bootstrap text:\n{}", bootstrap_text));
  }
  return bootstrap;
}

class CreateBootstrapConfigurationTest : public testing::Test {
protected:
  CreateBootstrapConfigurationTest() = default;

  // Sets mock expectations when the code under test resolves the URIs provided in the options.
  void setupUriResolutionExpectations() {
    // TODO(zhangtom): This needs to be fixed. 
    ON_CALL(mock_dispatcher_, createDnsResolver(_, _)).WillByDefault(Return(mock_resolver_));

    EXPECT_CALL(*mock_resolver_, resolve(_, _, _))
        .WillRepeatedly(Invoke([](const std::string&, Envoy::Network::DnsLookupFamily,
                                  // Even though clang-tidy is right, we cannot
                                  // change the function declaration here.
                                  // NOLINTNEXTLINE(performance-unnecessary-value-param)
                                  Envoy::Network::DnsResolver::ResolveCb callback) {
          callback(Envoy::Network::DnsResolver::ResolutionStatus::Success,
                   Envoy::TestUtility::makeDnsResponse({"127.0.0.1"}));
          return nullptr;
        }));
  }

  std::shared_ptr<Envoy::Network::MockDnsResolver> mock_resolver_{
      std::make_shared<Envoy::Network::MockDnsResolver>()};
  NiceMock<Envoy::Event::MockDispatcher> mock_dispatcher_;
  int number_of_workers_{1};
};

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH1) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl("nighthawk_client http://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH1RespectingPortInUri) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl("nighthawk_client http://www.example.org:81");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 81
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH1WithMultipleTargets) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options = Client::TestUtility::createOptionsImpl(
      "nighthawk_client --multi-target-endpoint www.example.org:80 --multi-target-endpoint "
      "www.example2.org:80 --multi-target-path /");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH1WithTls) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl("nighthawk_client https://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        transport_socket {
          name: "envoy.transport_sockets.tls"
          typed_config {
            [type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext] {
              common_tls_context {
                alpn_protocols: "http/1.1"
              }
              sni: "www.example.org"
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 443
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH1AndMultipleWorkers) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl("nighthawk_client http://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
      clusters {
        name: "1"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "1"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, /* number_of_workers = */ 2);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH2) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl("nighthawk_client --h2 http://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http2_protocol_options {
                  max_concurrent_streams {
                    value: 2147483647
                  }
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH2WithTls) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl("nighthawk_client --h2 https://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        transport_socket {
          name: "envoy.transport_sockets.tls"
          typed_config {
            [type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext] {
              common_tls_context {
                alpn_protocols: "h2"
              }
              sni: "www.example.org"
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 443
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http2_protocol_options {
                  max_concurrent_streams {
                    value: 2147483647
                  }
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapForH3) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options = Client::TestUtility::createOptionsImpl(
      "nighthawk_client --protocol http3 https://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        transport_socket {
          name: "envoy.transport_sockets.quic"
          typed_config {
            [type.googleapis.com/envoy.extensions.transport_sockets.quic.v3.QuicUpstreamTransport] {
              upstream_tls_context {
                common_tls_context {
                  alpn_protocols: "h3"
                }
                sni: "www.example.org"
              }
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 443
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http3_protocol_options {
                  quic_protocol_options {
                    max_concurrent_streams {
                      value: 2147483647
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapWithRequestSourceAndCustomTimeout) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options = Client::TestUtility::createOptionsImpl(
      "nighthawk_client --timeout 10 http://www.example.org --request-source 127.0.0.1:6000");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 10
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
      clusters {
        name: "0.requestsource"
        type: STATIC
        connect_timeout {
          seconds: 10
        }
        load_assignment {
          cluster_name: "0.requestsource"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 6000
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              explicit_http_config {
                http2_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapWithRequestSourceAndMultipleWorkers) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options = Client::TestUtility::createOptionsImpl(
      "nighthawk_client --timeout 10 http://www.example.org --request-source 127.0.0.1:6000");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 10
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
      clusters {
        name: "0.requestsource"
        type: STATIC
        connect_timeout {
          seconds: 10
        }
        load_assignment {
          cluster_name: "0.requestsource"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 6000
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              explicit_http_config {
                http2_protocol_options {
                }
              }
            }
          }
        }
      }
      clusters {
        name: "1"
        type: STATIC
        connect_timeout {
          seconds: 10
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "1"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
      clusters {
        name: "1.requestsource"
        type: STATIC
        connect_timeout {
          seconds: 10
        }
        load_assignment {
          cluster_name: "1.requestsource"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 6000
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              explicit_http_config {
                http2_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, /* number_of_workers = */ 2);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapWithCustomOptions) {
  setupUriResolutionExpectations();

  const std::string stats_sink_json =
      "{name:\"envoy.stat_sinks.statsd\",typed_config:{\"@type\":\"type."
      "googleapis.com/"
      "envoy.config.metrics.v3.StatsdSink\",tcp_cluster_name:\"statsd\"}}";

  const std::string tls_context_json = "{common_tls_context:{tls_params:{"
                                       "cipher_suites:[\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"]}}}";

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl(fmt::format("nighthawk_client "
                                                         "--max-pending-requests 10 "
                                                         "--stats-sinks {} "
                                                         "--stats-flush-interval 20 "
                                                         "--tls-context {} "
                                                         "https://www.example.org",
                                                         stats_sink_json, tls_context_json));

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 10
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        transport_socket {
          name: "envoy.transport_sockets.tls"
          typed_config {
            [type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext] {
              common_tls_context {
                tls_params {
                  cipher_suites: "-ALL:ECDHE-RSA-AES256-GCM-SHA384"
                }
                alpn_protocols: "http/1.1"
              }
              sni: "www.example.org"
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 443
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_sinks {
      name: "envoy.stat_sinks.statsd"
      typed_config {
        [type.googleapis.com/envoy.config.metrics.v3.StatsdSink] {
          tcp_cluster_name: "statsd"
        }
      }
    }
    stats_flush_interval {
      seconds: 20
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapSetsMaxRequestToAtLeastOne) {
  setupUriResolutionExpectations();

  // The tested behavior is that even though we set --max-pending-requests 0,
  // the code will configure a value of 1.
  std::unique_ptr<Client::OptionsImpl> options = Client::TestUtility::createOptionsImpl(
      "nighthawk_client --max-pending-requests 0 http://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 80
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, CreatesBootstrapWithCustomTransportSocket) {
  setupUriResolutionExpectations();

  const std::string transport_socket_json =
      "{name:\"envoy.transport_sockets.tls\","
      "typed_config:{\"@type\":\"type.googleapis.com/"
      "envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext\","
      "common_tls_context:{tls_params:{"
      "cipher_suites:[\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"]}}}}";

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl(fmt::format("nighthawk_client "
                                                         "--transport-socket {} "
                                                         "https://www.example.org",
                                                         transport_socket_json));

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        transport_socket {
          name: "envoy.transport_sockets.tls"
          typed_config {
            [type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext] {
              common_tls_context {
                tls_params {
                  cipher_suites: "-ALL:ECDHE-RSA-AES256-GCM-SHA384"
                }
              }
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 443
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

TEST_F(CreateBootstrapConfigurationTest, DeterminesSniFromRequestHeader) {
  setupUriResolutionExpectations();

  std::unique_ptr<Client::OptionsImpl> options =
      Client::TestUtility::createOptionsImpl("nighthawk_client "
                                             "--request-header Host:test.example.com "
                                             "https://www.example.org");

  absl::StatusOr<Bootstrap> expected_bootstrap = parseBootstrapFromText(R"pb(
    static_resources {
      clusters {
        name: "0"
        type: STATIC
        connect_timeout {
          seconds: 30
        }
        circuit_breakers {
          thresholds {
            max_connections {
              value: 100
            }
            max_pending_requests {
              value: 1
            }
            max_requests {
              value: 100
            }
            max_retries {
            }
          }
        }
        transport_socket {
          name: "envoy.transport_sockets.tls"
          typed_config {
            [type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext] {
              common_tls_context {
                alpn_protocols: "http/1.1"
              }
              sni: "test.example.com"
            }
          }
        }
        load_assignment {
          cluster_name: "0"
          endpoints {
            lb_endpoints {
              endpoint {
                address {
                  socket_address {
                    address: "127.0.0.1"
                    port_value: 443
                  }
                }
              }
            }
          }
        }
        typed_extension_protocol_options {
          key: "envoy.extensions.upstreams.http.v3.HttpProtocolOptions"
          value {
            [type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions] {
              common_http_protocol_options {
                max_requests_per_connection {
                  value: 4294937295
                }
              }
              explicit_http_config {
                http_protocol_options {
                }
              }
            }
          }
        }
      }
    }
    stats_flush_interval {
      seconds: 5
    }
  )pb");
  ASSERT_THAT(expected_bootstrap, StatusIs(absl::StatusCode::kOk));

  absl::StatusOr<Bootstrap> bootstrap =
      createBootstrapConfiguration(mock_dispatcher_, *options, number_of_workers_);
  ASSERT_THAT(bootstrap, StatusIs(absl::StatusCode::kOk));
  EXPECT_THAT(*bootstrap, EqualsProto(*expected_bootstrap));

  // Ensure the generated bootstrap is valid.
  Envoy::MessageUtil::validate(*bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor());
}

} // namespace
} // namespace Nighthawk
