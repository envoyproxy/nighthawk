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

  Note that it derives from NighthawkTestServer, as that
  is implemented as a customized Envoy, which is convenient here: the CLI and admin interface
  mechanics that we rely on are the same. So all we do here, is specialize so we can override
  the docker image and binary name.

  Attributes:
    See base class
  """

  def __init__(self, config_template_path, server_ip, ip_version, parameters=dict(), tag=""):
    """Initialize an EnvoyProxyServer instance.

    Arguments:
      config_template_path: Path to the configuration template.
      server_ip: IP address of that the server should use
      ip_version: IP version that the server should use when listening
      parameters: Optional dictionary. Use (optional)
      tag: (optional)
    """
    # If no explicit envoy path is passed, we'll use nighthawk_test_server.
    super(EnvoyProxyServer, self).__init__(
        os.getenv("ENVOY_PATH", "nighthawk_test_server"),
        config_template_path,
        server_ip,
        ip_version,
        parameters=parameters,
        tag=tag)
    self.docker_image = os.getenv("ENVOY_DOCKER_IMAGE_TO_TEST", "")


@pytest.fixture()
def proxy_config():
  """Yield a single Envoy proxy configuration."""
  yield "nighthawk/benchmarks/configurations/envoy_proxy.yaml"


class InjectHttpProxyIntegrationTestBase(HttpIntegrationTestBase):
  """Proxy and Test server fixture.

  Fixture which spins up a Nighthawk test server as well as an Envoy proxy
  which directs traffic to that. Both will be listing for plain http traffic.
  """

  def __init__(self, ip_version, server_config, proxy_config):
    """Initialize an InjectHttpProxyIntegrationTestBase.

    Arguments:
      ip_version: Use ipv4 or ipv6
      server_config: Path to the server configuration.
      proxy_config: Path to the proxy configuration.
    """
    super(InjectHttpProxyIntegrationTestBase, self).__init__(ip_version, server_config)
    self.proxy_config = proxy_config

  def setUp(self):
    """Set up the injected Envoy proxy as well as the test server.

    Assert that both started successfully, and return afterwards.
    """
    super(InjectHttpProxyIntegrationTestBase, self).setUp()
    logging.info("injecting envoy proxy ...")
    # TODO(oschaaf): how should this interact with multiple backends?
    self.parameters["proxy_ip"] = self.test_server.server_ip
    self.parameters["server_port"] = self.test_server.server_port
    proxy_server = EnvoyProxyServer(
        self.proxy_config,
        self.server_ip,
        self.ip_version,
        parameters=self.parameters,
        tag=self.tag)
    assert (proxy_server.start())
    logging.info("envoy proxy listening at {ip}:{port}".format(
        ip=proxy_server.server_ip, port=proxy_server.server_port))
    self.proxy_server = proxy_server

  def tearDown(self):
    """Tear down the proxy and test server. Assert that both exit succesfully."""
    super(InjectHttpProxyIntegrationTestBase, self).tearDown()
    assert (self.proxy_server.stop() == 0)

  def getTestServerRootUri(self):
    """Get the root uri, pointing to the proxy address and port."""
    r = super(InjectHttpProxyIntegrationTestBase, self).getTestServerRootUri()
    # TODO(oschaaf): fix, kind of a hack.
    r = r.replace(":%s" % self.test_server.server_port, ":%s" % self.proxy_server.server_port)
    return r


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def inject_envoy_http_proxy_fixture(request, server_config, proxy_config):
  """Injects an Envoy proxy in front of the test server.

  NOTE: Depends on the proxy_config fixture, which must be explicitly imported
  into the consuming module when using this fixture.
  """
  f = InjectHttpProxyIntegrationTestBase(request.param, server_config, proxy_config)
  f.setUp()
  yield f
  f.tearDown()
