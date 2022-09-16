"""Customized fixture to dynamically configure an Envoy Proxy."""

import pytest
import logging
from typing import Generator

from test.integration import integration_test_fixtures, common, utility
import envoy_proxy
from dynamic_config import dynamic_config_server
from collections.abc import Callable
from google.protobuf import message


@pytest.fixture()
def proxy_config() -> Generator[str, None, None]:
  """Yield the stock Envoy proxy configuration."""
  yield "nighthawk/benchmarks/configurations/envoy_proxy.yaml"


class InjectDynamicHttpProxyIntegrationTestBase(envoy_proxy.InjectHttpProxyIntegrationTestBase):
  """Proxy and Test server fixture.

  Fixture which spins up a Nighthawk test server, an Envoy proxy and a xDS configuration
  task which dynamically configure the Envoy. Both will be listening for plain http traffic.

  Attributes:
    See base class.
  """

  def __init__(self, request, server_config: str, proxy_config: str,
               dynamic_config_generator: Callable[[str, list[utility.SocketAddress]],
                                                  message.Message]):
    """Initialize an InjectDynamicHttpProxyIntegrationTestBase.

    Arguments:
      request: The pytest `request` test fixture used to determine information
        about the currently executing test case.
      server_config: Path to the server configuration.
      proxy_config: Path to the proxy configuration.
      dynamic_config_generator: Function for generating settings for the dynamic configuration component.
    """
    super(InjectDynamicHttpProxyIntegrationTestBase, self).__init__(request, server_config,
                                                                    proxy_config)
    self._dynamic_config_generator = dynamic_config_generator

  def setUp(self):
    """Set up the injected Envoy proxy as well as the test server.

    Assert that both started successfully, and return afterwards.
    """
    super(InjectDynamicHttpProxyIntegrationTestBase, self).setUp()

    available_endpoints = utility.parseUrisToSocketAddress(self.getAllTestServerRootUris())
    test_dir = self.test_server.tmpdir
    dynamic_config_settings = self._dynamic_config_generator(test_dir, available_endpoints)

    self._dynamic_config_controller = dynamic_config_server.DynamicConfigController(
        dynamic_config_settings)
    assert (self._dynamic_config_controller.start())
    logging.info("dynamic configuration running")

  def tearDown(self, caplog):
    """Tear down the proxy and test server. Assert that both exit succesfully."""
    super(InjectDynamicHttpProxyIntegrationTestBase, self).tearDown(caplog)
    self._dynamic_config_controller.stop()


@pytest.fixture(params=integration_test_fixtures.determineIpVersionsFromEnvironment())
def inject_dynamic_envoy_http_proxy_fixture(request, server_config, proxy_config,
                                            dynamic_config_generator, caplog):
  """Injects a dynamically configured Envoy proxy in front of the test server.

  Arguments:
    request: The pytest `request` test fixture used to determine information about the currently executing test case.
    server_config: Path to the server configuration template.
    proxy_config: Path to the proxy configuration template.
    dynamic_config_binary_path: Path to the dynamic configuration binary.
    dynamic_config_generator: Function used to generate the settings for the configuration generator.
    caplog: The pytest `caplog` test fixture used to examine logged messages.

  Yields: a successfully set up InjectDynamicHttpProxyIntegrationTestBase
  instance.
  """
  fixture = InjectDynamicHttpProxyIntegrationTestBase(request, server_config, proxy_config,
                                                      dynamic_config_generator)
  fixture.setUp()
  yield fixture
  fixture.tearDown(caplog)
