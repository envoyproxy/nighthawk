"""Customized fixture to dynamically configure an Envoy Proxy."""

import pytest
import logging
import os
from typing import Generator

from test.integration import integration_test_fixtures
import envoy_proxy
from dynamic_config import dynamic_config_server
from nighthawk.api.configuration import cluster_config_manager_pb2


@pytest.fixture()
def proxy_config() -> Generator[str, None, None]:
  """Yield the stock Envoy proxy configuration."""
  yield "nighthawk/benchmarks/configurations/envoy_proxy.yaml"


# TODO(kbaichoo): Stubbed implementation. Will be enhanced to
# leverage backend addresses.
@pytest.fixture()
def dynamic_config_settings(
) -> Generator[cluster_config_manager_pb2.DynamicClusterConfigManagerSettings, None, None]:
  """Yield the stock Envoy proxy configuration."""
  settings = cluster_config_manager_pb2.DynamicClusterConfigManagerSettings()
  settings.refresh_interval.seconds = 5
  settings.output_file = 'new_cds.pb'
  cluster = settings.clusters.add()
  cluster.name = 'service_envoyproxy_io'
  yield settings


class InjectDynamicHttpProxyIntegrationTestBase(envoy_proxy.InjectHttpProxyIntegrationTestBase):
  """Proxy and Test server fixture.

  Fixture which spins up a Nighthawk test server, an Envoy proxy and a xDS configuration
  task which dynamically configure the Envoy. Both will be listening for plain http traffic.

  Attributes:
    See base class.
  """

  def __init__(
      self, request, server_config: str, proxy_config: str,
      dynamic_config_settings: cluster_config_manager_pb2.DynamicClusterConfigManagerSettings):
    """Initialize an InjectDynamicHttpProxyIntegrationTestBase.

    Arguments:
      request: The pytest `request` test fixture used to determine information
        about the currently executing test case.
      server_config: Path to the server configuration.
      proxy_config: Path to the proxy configuration.
      dynamic_config_settings: Settings for the dynamic configuration component.
    """
    super(InjectDynamicHttpProxyIntegrationTestBase, self).__init__(request, server_config,
                                                                    proxy_config)
    self._dynamic_config_settings = dynamic_config_settings

  def setUp(self):
    """Set up the injected Envoy proxy as well as the test server.

    Assert that both started successfully, and return afterwards.
    """
    super(InjectDynamicHttpProxyIntegrationTestBase, self).setUp()

    output_file = os.path.join(self.test_server.tmpdir, self._dynamic_config_settings.output_file)
    self._dynamic_config_settings.output_file = output_file
    logging.info(f"Injecting dynamic configuration. Output file: {output_file}")

    # TODO(kbaichoo): we only hardcode a single endpoint, but will expand on this.
    endpoints = self._dynamic_config_settings.clusters[0].endpoints.add()
    endpoints.ip = self.test_server.server_ip
    endpoints.port = self.test_server.server_port

    self._dynamic_config_controller = dynamic_config_server.DynamicConfigController(
        self._dynamic_config_settings)

    assert (self._dynamic_config_controller.start())
    logging.info("dynamic configuration running")

  def tearDown(self, caplog):
    """Tear down the proxy and test server. Assert that both exit succesfully."""
    super(InjectDynamicHttpProxyIntegrationTestBase, self).tearDown(caplog)
    self._dynamic_config_controller.stop()


@pytest.fixture(params=integration_test_fixtures.determineIpVersionsFromEnvironment())
def inject_dynamic_envoy_http_proxy_fixture(request, server_config, proxy_config,
                                            dynamic_config_settings, caplog):
  """Injects a dynamically configured Envoy proxy in front of the test server.

  Arguments:
    request: The pytest `request` test fixture used to determine information about the currently executing test case.
    server_config: path to the server configuration template.
    proxy_config: path to the proxy configuration template.
    dynamic_config_binary_path: path to the dynamic configuration binary.
    dynamic_config_settings: settings used for the dynamic configuration.
    caplog: The pytest `caplog` test fixture used to examine logged messages.

  Yields: a successfully set up InjectDynamicHttpProxyIntegrationTestBase
  instance.
  """
  fixture = InjectDynamicHttpProxyIntegrationTestBase(request, server_config, proxy_config,
                                                      dynamic_config_settings)
  fixture.setUp()
  yield fixture
  fixture.tearDown(caplog)
