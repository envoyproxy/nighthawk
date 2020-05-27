#!/usr/bin/env python3
"""@package infra.py

Contains customized fixture & EnvoyProxyServer abstraction for use in tests.
"""

import logging
import os
import pytest

from test.integration.integration_test_fixtures import (HttpIntegrationTestBase,
                                                        determineIpVersionsFromEnvironment)
from test.integration.nighthawk_test_server import NighthawkTestServer

class EnvoyProxyServer(NighthawkTestServer):
  """
  Envoy proxy server abstraction. Note that it derives from NighthawkTestServer, as that
  is implemented as a custimized Envoy, which is convenient here: the CLI and admin interface
  mechanics that we rely on are the same. So all we do here, is specialize so we can override
  the docker image and binary name.
  """
  def __init__(self,
               config_template_path,
               server_ip,
               ip_version,
               parameters=dict(),
               tag=""):
    super(EnvoyProxyServer, self).__init__("envoy", config_template_path, server_ip,
                                              ip_version, parameters=parameters, tag=tag)
    self.docker_image = os.getenv("ENVOY_DOCKER_IMAGE_TO_TEST")

class InjectHttpProxyIntegrationTestBase(HttpIntegrationTestBase):
  """
  Fixture which spins up a Nighthawk test server which listens on plain http,
  and set up an Envoy instances to proxy to that, als listening on plain http.
  """

  def __init__(self, ip_version):
    """See base class."""
    super(InjectHttpProxyIntegrationTestBase, self).__init__(ip_version)

  def setUp(self):
    super(InjectHttpProxyIntegrationTestBase, self).setUp()
    logging.info("injecting envoy proxy ...")
    # TODO(oschaaf): how should this interact with multiple backends?
    self.parameters["proxy_ip"] = self.test_server.server_ip
    self.parameters["server_port"] = self.test_server.server_port
    proxy_server = EnvoyProxyServer(os.path.join(self.test_rundir, "benchmarks/configurations/envoy_proxy.yaml"), self.server_ip,
                                    self.ip_version, parameters=self.parameters, tag=self.tag)
    assert (proxy_server.start())
    logging.info("envoy proxy listening at {ip}:{port}".format(ip=proxy_server.server_ip, port=proxy_server.server_port))
    self.proxy_server = proxy_server

  def getTestServerRootUri(self):
    """See base class."""
    r = super(InjectHttpProxyIntegrationTestBase, self).getTestServerRootUri()
    # TODO(oschaaf): fix, kind of a hack.
    r = r.replace(":%s" % self.test_server.server_port, ":%s" % self.proxy_server.server_port)
    return r

@pytest.fixture(params=determineIpVersionsFromEnvironment())
def inject_envoy_http_proxy_fixture(request):
  '''
  Injects an Envoy proxy.
  '''
  f = InjectHttpProxyIntegrationTestBase(request.param)
  f.setUp()
  yield f
  f.tearDown()
