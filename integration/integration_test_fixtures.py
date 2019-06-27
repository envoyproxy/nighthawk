"""@package integration_test_fixtures
Base classes for Nighthawk integration tests
"""

import json
import logging
import os
import socket
import subprocess
import sys
import threading
import time
import unittest

from common import IpVersion
from nighthawk_test_server import NighthawkTestServer


class IntegrationTestBase(unittest.TestCase):
  """
    IntegrationTestBase facilitates testing against the Nighthawk test server, by determining a free port, and starting it up in a separate process in setUp().
    """

  def __init__(self, *args, **kwargs):
    super(IntegrationTestBase, self).__init__(*args, **kwargs)
    self.test_rundir = os.path.join(os.environ["TEST_SRCDIR"], os.environ["TEST_WORKSPACE"])

    self.nighthawk_test_server_path = os.path.join(self.test_rundir, "nighthawk_test_server")
    self.nighthawk_test_config_path = None
    self.nighthawk_client_path = os.path.join(self.test_rundir, "nighthawk_client")
    self.server_ip = "::1" if IntegrationTestBase.ip_version == IpVersion.IPV6 else "127.0.0.1"
    self.socket_type = socket.AF_INET6 if IntegrationTestBase.ip_version == IpVersion.IPV6 else socket.AF_INET
    self.test_server = None
    self.server_port = -1
    self.parameters = dict()

  # TODO(oschaaf): For the NH test server, add a way to let it determine a port by itself and pull that
  # out.
  def getFreeListenerPortForAddress(self, address):
    """
    Determines a free port and returns that. Theoretically it is possible that another process
    will steal the port before our caller is able to leverage it, but we take that chance.
    The upside is that we can push the port upon the server we are about to start through configuration
    which is compatible accross servers.
    """
    # TODO(oschaaf): When we are on python 3 use RAII here.
    sock = socket.socket(self.socket_type, socket.SOCK_STREAM)
    sock.bind((address, 0))
    port = sock.getsockname()[1]
    sock.close()
    return port

  def setUp(self):
    """
    Performs sanity checks and starts up the server. Upon exit the server is ready to accept connections.
    """
    self.assertTrue(os.path.exists(self.nighthawk_test_server_path))
    self.assertTrue(os.path.exists(self.nighthawk_client_path))
    self.server_port = self.getFreeListenerPortForAddress(self.server_ip)
    self.test_server = NighthawkTestServer(
        self.nighthawk_test_server_path, self.nighthawk_test_config_path, self.server_ip,
        self.server_port, IntegrationTestBase.ip_version, self.parameters)
    self.assertTrue(self.test_server.start())

  def tearDown(self):
    """
    Stops the server.
    """
    self.assertEqual(0, self.test_server.stop())

  def getNighthawkCounterMapFromJson(self, parsed_json):
    """
    Utility method to get the counters from the json indexed by name.
    Note:the `client.` prefix will be stripped from the index.
    """
    return {
        counter["name"][len("client."):]: int(counter["value"])
        for counter in parsed_json["results"][0]["counters"]
    }

  def getTestServerRootUri(self, https=False):
    """
    Utility for getting the http://host:port/ that can be used to query the server we started in setUp()
    """
    uri_host = self.server_ip
    if IntegrationTestBase.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip

    uri = "%s://%s:%s/" % ("https" if https else "http", uri_host, self.test_server.server_port)
    return uri

  def runNighthawk(self, args, expect_failure=False, timeout=30):
    """
    Runs Nighthawk against the test server, returning a json-formatted result.
    If the timeout is exceeded an exception will be raised.
    """
    if IntegrationTestBase.ip_version == IpVersion.IPV6:
      args.insert(0, "--address-family v6")
    args.insert(0, "--output-format json")
    args.insert(0, self.nighthawk_client_path)
    logging.info("Nighthawk client popen() args: [%s]" % args)
    client_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = client_process.communicate()
    logging.info(stderr.decode('utf-8'))
    json_string = stdout.decode('utf-8')
    if expect_failure:
      self.assertNotEqual(0, client_process.returncode)
    else:
      self.assertEqual(0, client_process.returncode)
      return json.loads(json_string)


# We don't have a straightforward way to do parameterized tests yet.
# But the tests can be run twice if desired so, and setting this to true will enable ipv6 mode.
IntegrationTestBase.ip_version = IpVersion.UNKNOWN


class HttpIntegrationTestBase(IntegrationTestBase):
  """
  Base for running plain http tests against the Nighthawk test server
  """

  def __init__(self, *args, **kwargs):
    super(HttpIntegrationTestBase, self).__init__(*args, **kwargs)
    self.nighthawk_test_config_path = os.path.join(
        self.test_rundir, "integration/configurations/nighthawk_http_origin.yaml")

  def getTestServerRootUri(self):
    return super(HttpIntegrationTestBase, self).getTestServerRootUri(False)


class HttpsIntegrationTestBase(IntegrationTestBase):
  """
  Base for https tests against the Nighthawk test server
  """

  def __init__(self, *args, **kwargs):
    super(HttpsIntegrationTestBase, self).__init__(*args, **kwargs)
    self.parameters["ssl_key_path"] = os.path.join(
        self.test_rundir, "external/envoy/test/config/integration/certs/serverkey.pem")
    self.parameters["ssl_cert_path"] = os.path.join(
        self.test_rundir, "external/envoy/test/config/integration/certs/servercert.pem")
    self.nighthawk_test_config_path = os.path.join(
        self.test_rundir, "integration/configurations/nighthawk_https_origin.yaml")

  def getTestServerRootUri(self):
    return super(HttpsIntegrationTestBase, self).getTestServerRootUri(True)
