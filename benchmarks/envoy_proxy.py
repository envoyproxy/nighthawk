#!/usr/bin/env python3
"""@package envoy_proxy.

Contains customized fixture & EnvoyProxyServer abstraction for use in tests.
"""

import logging
import os
import pytest

from test.integration.integration_test_fixtures import (HttpIntegrationTestBase,
                                                        determineIpVersionsFromEnvironment)
from test.integration.nighthawk_test_server import NighthawkTestServer


class EnvoyProxyServer(NighthawkTestServer):
  """Envoy proxy server abstraction.

  Note that it derives from NighthawkTestServer, as that is implemented as a customized
  Envoy, which is convenient here: the CLI and admin interface mechanics that we rely on
  are the same. So all we do here, is specialize so we can override the docker image and
  binary name.

  Attributes:
    See base class

  Example:
    See InjectHttpProxyIntegrationTestBase below for usage.
  """

  def __init__(self,
               config_template_path,
               server_ip,
               ip_version,
               request,
               parameters=dict(),
               tag=""):
    """Initialize an EnvoyProxyServer instance.

    Arguments:
      config_template_path: Configuration template for the proxy.
      server_ip: IP address for the proxy to use.
      ip_version: IP version that the proxy should use when listening.
      request: The pytest `request` test fixture used to determine information
        about the currently executing test case.
      parameters: Dictionary. Supply this to provide template parameter replacement values (optional).
      tag: String. Supply this to get recognizeable output locations (optional).
    """
    # If no explicit envoy path is passed, we'll use nighthawk_test_server.
    super(EnvoyProxyServer, self).__init__(os.getenv("ENVOY_PATH", "nighthawk_test_server"),
                                           config_template_path,
                                           server_ip,
                                           ip_version,
                                           request,
                                           parameters=parameters,
                                           tag=tag)
    self.docker_image = os.getenv("ENVOY_DOCKER_IMAGE_TO_TEST", "")


@pytest.fixture()
def proxy_config():
  """Yield the stock Envoy proxy configuration."""
  yield "nighthawk/benchmarks/configurations/envoy_proxy.yaml"


class InjectHttpProxyIntegrationTestBase(HttpIntegrationTestBase):
  """Proxy and Test server fixture.

  Fixture which spins up a Nighthawk test server as well as an Envoy proxy
  which directs traffic to that. Both will be listing for plain http traffic.
  """

  def __init__(self, request, server_config, proxy_config):
    """Initialize an InjectHttpProxyIntegrationTestBase.

    Arguments:
      request: The pytest `request` test fixture used to determine information
        about the currently executing test case.
      server_config: Path to the server configuration.
      proxy_config: Path to the proxy configuration.
    """
    super(InjectHttpProxyIntegrationTestBase, self).__init__(request, server_config)
    self._proxy_config = proxy_config

  def setUp(self):
    """Set up the injected Envoy proxy as well as the test server.

    Assert that both started successfully, and return afterwards.
    """
    super(InjectHttpProxyIntegrationTestBase, self).setUp()
    logging.info("injecting envoy proxy ...")
    # TODO(oschaaf): how should this interact with multiple backends?
    self.parameters["proxy_ip"] = self.test_server.server_ip
    self.parameters["server_port"] = self.test_server.server_port
    proxy_server = EnvoyProxyServer(self._proxy_config,
                                    self.server_ip,
                                    self.ip_version,
                                    self.request,
                                    parameters=self.parameters,
                                    tag=self.tag)
    assert (proxy_server.start())
    logging.info("envoy proxy listening at {ip}:{port}".format(ip=proxy_server.server_ip,
                                                               port=proxy_server.server_port))
    self.proxy_server = proxy_server

  def tearDown(self, caplog):
    """Tear down the proxy and test server. Assert that both exit succesfully."""
    super(InjectHttpProxyIntegrationTestBase, self).tearDown(caplog)
    assert (self.proxy_server.stop() == 0)

  def getTestServerRootUri(self):
    """Get the root uri, pointing to the proxy address and port."""
    root_uri = super(InjectHttpProxyIntegrationTestBase, self).getTestServerRootUri()
    root_uri = root_uri.replace(":%s" % self.test_server.server_port,
                                ":%s" % self.proxy_server.server_port)
    return root_uri


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def inject_envoy_http_proxy_fixture(request, server_config, proxy_config, caplog):
  """Injects an Envoy proxy in front of the test server.

  NOTE: Depends on the proxy_config fixture, which must be explicitly imported
  into the consuming module when using this fixture.

  Arguments:
    request: supplies the ip version.
    server_config: path to the server configuration template.
    proxy_config: path to the proxy configuration template.
    caplog: The pytest `caplog` test fixture used to examine logged messages.

  Yields: a successfully set up InjectHttpProxyIntegrationTestBase instance.
  """
  fixture = InjectHttpProxyIntegrationTestBase(request, server_config, proxy_config)
  fixture.setUp()
  yield fixture
  fixture.tearDown(caplog)
