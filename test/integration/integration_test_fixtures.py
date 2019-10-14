"""@package integration_test_fixtures
Base classes for Nighthawk integration tests
"""

import json
import logging
import os
import requests
import socket
import subprocess
import sys
import threading
import time
import pytest

from test.integration.common import IpVersion, NighthawkException
from test.integration.nighthawk_test_server import NighthawkTestServer
from test.integration.nighthawk_grpc_service import NighthawkGrpcService


def determineIpVersionsFromEnvironment():
  env_versions = os.environ.get("ENVOY_IP_TEST_VERSIONS", "all")
  if env_versions == "v4only":
    versions = [IpVersion.IPV4]
  elif env_versions == "v6only":
    versions = [IpVersion.IPV6]
  elif env_versions == "all":
    versions = [IpVersion.IPV4, IpVersion.IPV6]
  else:
    raise NighthawkException("Unknown ip version: '%s'" % versions)
  return versions


class IntegrationTestBase():
  """
  IntegrationTestBase facilitates testing against the Nighthawk test server, by determining a free port, and starting it up in a separate process in setUp().
  """

  def __init__(self, ip_version):
    super(IntegrationTestBase, self).__init__()
    self.test_rundir = os.path.join(os.environ["TEST_SRCDIR"], os.environ["TEST_WORKSPACE"])
    self.nighthawk_test_server_path = os.path.join(self.test_rundir, "nighthawk_test_server")
    self.nighthawk_test_config_path = None
    self.nighthawk_client_path = os.path.join(self.test_rundir, "nighthawk_client")
    assert ip_version != IpVersion.UNKNOWN
    self.server_ip = "::1" if ip_version == IpVersion.IPV6 else "127.0.0.1"
    self.socket_type = socket.AF_INET6 if ip_version == IpVersion.IPV6 else socket.AF_INET
    self.test_server = None
    self.parameters = {}
    self.ip_version = ip_version
    self.grpc_service = None

  # TODO(oschaaf): For the NH test server, add a way to let it determine a port by itself and pull that
  # out.
  def getFreeListenerPortForAddress(self, address):
    """
    Determines a free port and returns that. Theoretically it is possible that another process
    will steal the port before our caller is able to leverage it, but we take that chance.
    The upside is that we can push the port upon the server we are about to start through configuration
    which is compatible accross servers.
    """
    with socket.socket(self.socket_type, socket.SOCK_STREAM) as sock:
      sock.bind((address, 0))
      port = sock.getsockname()[1]
    return port

  def setUp(self):
    """
    Performs sanity checks and starts up the server. Upon exit the server is ready to accept connections.
    """
    assert (os.path.exists(self.nighthawk_test_server_path))
    assert (os.path.exists(self.nighthawk_client_path))
    test_id = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0].replace(
        "[", "_").replace("]", "")
    self.parameters["test_id"] = test_id
    self.test_server = NighthawkTestServer(self.nighthawk_test_server_path,
                                           self.nighthawk_test_config_path, self.server_ip,
                                           self.ip_version, self.parameters)
    assert (self.test_server.start())

  def tearDown(self):
    """
    Stops the server.
    """
    assert (self.test_server.stop() == 0)
    if not self.grpc_service is None:
      assert (self.grpc_service.stop() == 0)

  def getNighthawkCounterMapFromJson(self, parsed_json):
    """
    Utility method to get the counters from the json indexed by name.
    """
    global_results_index = len(parsed_json["results"]) - 1
    return {
        counter["name"]: int(counter["value"])
        for counter in parsed_json["results"][global_results_index]["counters"]
    }

  def getNighthawkGlobalHistogramsbyIdFromJson(self, parsed_json):
    """
    Utility method to get the global histograms from the json indexed by id.
    """
    return {statistic["id"]: statistic for statistic in parsed_json["results"][0]["statistics"]}

  def getTestServerRootUri(self, https=False):
    """
    Utility for getting the http://host:port/ that can be used to query the server we started in setUp()
    """
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip

    uri = "%s://%s:%s/" % ("https" if https else "http", uri_host, self.test_server.server_port)
    return uri

  def getTestServerStatisticsJson(self):
    """
    Utility to grab a statistics snapshot from the test server.
    """
    return self.test_server.fetchJsonFromAdminInterface("/stats?format=json")

  def getServerStatFromJson(self, server_stats_json, name):
    counters = server_stats_json["stats"]
    for counter in counters:
      if counter["name"] == name:
        return int(counter["value"])
    return None

  def runNighthawkClient(self, args, expect_failure=False, timeout=30, as_json=True):
    """
    Runs Nighthawk against the test server, returning a json-formatted result
    and logs. If the timeout is exceeded an exception will be raised.
    """
    if self.ip_version == IpVersion.IPV6:
      args.insert(0, "--address-family v6")
    if as_json:
      args.insert(0, "--output-format json")
    args.insert(0, self.nighthawk_client_path)
    logging.info("Nighthawk client popen() args: [%s]" % args)
    client_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = client_process.communicate()
    logs = stderr.decode('utf-8')
    output = stdout.decode('utf-8')
    if expect_failure:
      assert (client_process.returncode == 0)
    else:
      if as_json:
        output = json.loads(output)
      assert (client_process.returncode == 0)
      return output, logs

  def assertIsSubset(self, subset, superset):
    self.assertLessEqual(subset.items(), superset.items())

  def startNighthawkGrpcService(self):
    host = self.server_ip if self.ip_version == IpVersion.IPV4 else "[%s]" % self.server_ip
    self.grpc_service = NighthawkGrpcService(
        os.path.join(self.test_rundir, "nighthawk_service"), host, self.ip_version)
    assert (self.grpc_service.start())


class HttpIntegrationTestBase(IntegrationTestBase):
  """
  Base for running plain http tests against the Nighthawk test server
  """

  def __init__(self, ip_version):
    super(HttpIntegrationTestBase, self).__init__(ip_version)
    self.nighthawk_test_config_path = os.path.join(
        self.test_rundir, "test/integration/configurations/nighthawk_http_origin.yaml")

  def getTestServerRootUri(self):
    return super(HttpIntegrationTestBase, self).getTestServerRootUri(False)


class HttpsIntegrationTestBase(IntegrationTestBase):
  """
  Base for https tests against the Nighthawk test server
  """

  def __init__(self, ip_version):
    super(HttpsIntegrationTestBase, self).__init__(ip_version)
    self.parameters["ssl_key_path"] = os.path.join(
        self.test_rundir, "external/envoy/test/config/integration/certs/serverkey.pem")
    self.parameters["ssl_cert_path"] = os.path.join(
        self.test_rundir, "external/envoy/test/config/integration/certs/servercert.pem")
    self.nighthawk_test_config_path = os.path.join(
        self.test_rundir, "test/integration/configurations/nighthawk_https_origin.yaml")

  def getTestServerRootUri(self):
    return super(HttpsIntegrationTestBase, self).getTestServerRootUri(True)


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def http_test_server_fixture(request):
  f = HttpIntegrationTestBase(request.param)
  f.setUp()
  yield f
  f.tearDown()


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def https_test_server_fixture(request):
  f = HttpsIntegrationTestBase(request.param)
  f.setUp()
  yield f
  f.tearDown()
