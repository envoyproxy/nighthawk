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

_TIMESTAMP = time.strftime('%Y-%m-%d-%H-%M-%S')


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
  IntegrationTestBase facilitates testing against the Nighthawk test server, by determining a free port,
  and starting it up in a separate process in setUp().

  Support for multiple test servers has been added in a way that minimizes impact to existing tests.
  self.test_server always points to the first test server, and methods assuming a single backend such
  as getTestServerRootUri were left intact. self._test_servers contains all test servers, including the
  first. Methods such as getTestServerRootUris that are aware of multiple test servers will also
  work when there is only one test server.

  This class will be refactored (https://github.com/envoyproxy/nighthawk/issues/258).
  """

  def __init__(self, ip_version, server_config, backend_count=1):
    """
    Args:
      ip_version: a single IP mode that this instance will test: IpVersion.IPV4 or IpVersion.IPV6
      server_config: path to the server configuration
      backend_count: number of Nighthawk Test Server backends to run, to allow testing MultiTarget mode
    Attributes:
      ip_version: IP version that the proxy should use when listening.
      server_ip: string containing the server ip that will be used to listen
      tag: String. Supply this to get recognizeable output locations.
      parameters: Dictionary. Supply this to provide template parameter replacement values.
      grpc_service: NighthawkGrpcService instance or None. Set by startNighthawkGrpcService().  
      test_server: NighthawkTestServer instance, set during setUp().
      nighthawk_client_path: String, path to the nighthawk_client binary.
    """
    super(IntegrationTestBase, self).__init__()
    assert ip_version != IpVersion.UNKNOWN
    self.ip_version = ip_version
    self.server_ip = "::/0" if ip_version == IpVersion.IPV6 else "0.0.0.0"
    self.server_ip = os.getenv("TEST_SERVER_EXTERNAL_IP", self.server_ip)
    self.tag = ""
    self.parameters = {}
    self.grpc_service = None
    self.test_server = None
    self.nighthawk_client_path = "nighthawk_client"
    self._nighthawk_test_server_path = "nighthawk_test_server"
    self._nighthawk_test_config_path = server_config
    self._nighthawk_service_path = "nighthawk_service"
    self._nighthawk_output_transform_path = "nighthawk_output_transform"
    self._socket_type = socket.AF_INET6 if ip_version == IpVersion.IPV6 else socket.AF_INET
    self._test_servers = []
    self._backend_count = backend_count
    self._test_id = ""

  # TODO(oschaaf): For the NH test server, add a way to let it determine a port by itself and pull that
  # out.
  def getFreeListenerPortForAddress(self, address):
    """
    Determines a free port and returns that. Theoretically it is possible that another process
    will steal the port before our caller is able to leverage it, but we take that chance.
    The upside is that we can push the port upon the server we are about to start through configuration
    which is compatible accross servers.
    """
    with socket.socket(self._socket_type, socket.SOCK_STREAM) as sock:
      sock.bind((address, 0))
      port = sock.getsockname()[1]
    return port

  def setUp(self):
    """
    Performs sanity checks and starts up the server. Upon exit the server is ready to accept connections.
    """
    if os.getenv("NH_DOCKER_IMAGE", "") == "":
      assert os.path.exists(
          self._nighthawk_test_server_path
      ), "Test server binary not found: '%s'" % self._nighthawk_test_server_path
      assert os.path.exists(self.nighthawk_client_path
                           ), "Nighthawk client binary not found: '%s'" % self.nighthawk_client_path

    self._test_id = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0].replace(
        "[", "_").replace("]", "").replace("/", "_")[5:]
    self.tag = "{timestamp}/{test_id}".format(timestamp=_TIMESTAMP, test_id=self._test_id)
    assert self._tryStartTestServers(), "Test server(s) failed to start"

  def tearDown(self):
    """
    Stops the server.
    """
    if not self.grpc_service is None:
      assert (self.grpc_service.stop() == 0)

    any_failed = False
    for test_server in self._test_servers:
      if test_server.stop() != 0:
        any_failed = True
    assert (not any_failed)

  def _tryStartTestServers(self):
    for i in range(self._backend_count):
      test_server = NighthawkTestServer(self._nighthawk_test_server_path,
                                        self._nighthawk_test_config_path,
                                        self.server_ip,
                                        self.ip_version,
                                        parameters=self.parameters,
                                        tag=self.tag)
      if not test_server.start():
        return False
      self._test_servers.append(test_server)
      if i == 0:
        self.test_server = test_server
    return True

  def getGlobalResults(self, parsed_json):
    """
    Utility to find the global/aggregated result in the json output
    """
    global_result = [x for x in parsed_json["results"] if x["name"] == "global"]
    assert (len(global_result) == 1)
    return global_result[0]

  def getNighthawkCounterMapFromJson(self, parsed_json):
    """
    Utility method to get the counters from the json indexed by name.
    """
    return {
        counter["name"]: int(counter["value"])
        for counter in self.getGlobalResults(parsed_json)["counters"]
    }

  def getNighthawkGlobalHistogramsbyIdFromJson(self, parsed_json):
    """
    Utility method to get the global histograms from the json indexed by id.
    """
    return {
        statistic["id"]: statistic for statistic in self.getGlobalResults(parsed_json)["statistics"]
    }

  def getTestServerRootUri(self, https=False):
    """
    Utility for getting the http://host:port/ that can be used to query the server we started in setUp()
    """
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip

    uri = "%s://%s:%s/" % ("https" if https else "http", uri_host, self.test_server.server_port)
    return uri

  def getAllTestServerRootUris(self, https=False):
    """
    Utility for getting the list of http://host:port/ that can be used to query the servers we started
    in setUp()
    """
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip

    return [
        "%s://%s:%s/" % ("https" if https else "http", uri_host, test_server.server_port)
        for test_server in self._test_servers
    ]

  def getTestServerStatisticsJson(self):
    """
    Utility to grab a statistics snapshot from the test server.
    """
    return self.test_server.fetchJsonFromAdminInterface("/stats?format=json")

  def getAllTestServerStatisticsJsons(self):
    """
    Utility to grab a statistics snapshot from multiple test servers.
    """
    return [
        test_server.fetchJsonFromAdminInterface("/stats?format=json")
        for test_server in self._test_servers
    ]

  def getServerStatFromJson(self, server_stats_json, name):
    """
    Utility to extract one statistic from a single json snapshot.
    """
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
    # Copy the args so our modifications to it stay local.
    args = args.copy()
    if os.getenv("NH_DOCKER_IMAGE", "") != "":
      args = [
          "docker", "run", "--network=host", "--rm",
          os.getenv("NH_DOCKER_IMAGE"), self.nighthawk_client_path
      ] + args
    else:
      args = [self.nighthawk_client_path] + args
    if self.ip_version == IpVersion.IPV6:
      args.append("--address-family v6")
    if as_json:
      args.append("--output-format json")
    logging.info("Nighthawk client popen() args: [%s]" % args)
    client_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = client_process.communicate()
    logs = stderr.decode('utf-8')
    output = stdout.decode('utf-8')
    logging.debug("Nighthawk client stdout: [%s]" % output)
    if logs:
      logging.debug("Nighthawk client stderr: [%s]" % logs)
    if as_json:
      output = json.loads(output)
    if expect_failure:
      assert (client_process.returncode != 0)
    else:
      assert (client_process.returncode == 0)
    return output, logs

  def transformNighthawkJson(self, json, format="human"):
    """Use to obtain one of the supported output from Nighthawk's raw json output.

    Arguments:
      json: String containing raw json output obtained via nighthawk_client --output-format=json
      format: String that specifies the desired output format. Must be one of [human|yaml|dotted-string|fortio]. Optional, defaults to "human".
    """

    # TODO(oschaaf): validate format arg.
    args = []
    if os.getenv("NH_DOCKER_IMAGE", "") != "":
      args = ["docker", "run", "--rm", "-i", os.getenv("NH_DOCKER_IMAGE")]
    args = args + [self._nighthawk_output_transform_path, "--output-format", format]
    logging.info("Nighthawk output transform popen() args: %s" % args)
    client_process = subprocess.Popen(args,
                                      stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
    logging.info("Nighthawk client popen() args: [%s]" % args)
    stdout, stderr = client_process.communicate(input=json.encode())
    logs = stderr.decode('utf-8')
    output = stdout.decode('utf-8')
    assert (client_process.returncode == 0)
    return stdout.decode('utf-8')

  def assertIsSubset(self, subset, superset):
    self.assertLessEqual(subset.items(), superset.items())

  def startNighthawkGrpcService(self, service_name="traffic-generator-service"):
    host = self.server_ip if self.ip_version == IpVersion.IPV4 else "[%s]" % self.server_ip
    self.grpc_service = NighthawkGrpcService(self._nighthawk_service_path, host, self.ip_version,
                                             service_name)
    assert (self.grpc_service.start())


class HttpIntegrationTestBase(IntegrationTestBase):
  """
  Base for running plain http tests against the Nighthawk test server
  NOTE: any script that consumes derivations of this, needs to needs also explictly
  import server_config, to avoid errors caused by the server_config not being found
  by pytest.
  """

  def __init__(self, ip_version, server_config):
    """See base class."""
    super(HttpIntegrationTestBase, self).__init__(ip_version, server_config)

  def getTestServerRootUri(self):
    """See base class."""
    return super(HttpIntegrationTestBase, self).getTestServerRootUri(False)


class MultiServerHttpIntegrationTestBase(IntegrationTestBase):
  """
  Base for running plain http tests against multiple Nighthawk test servers
  """

  def __init__(self, ip_version, server_config, backend_count):
    """See base class."""
    super(MultiServerHttpIntegrationTestBase, self).__init__(ip_version, server_config,
                                                             backend_count)

  def getTestServerRootUri(self):
    """See base class."""
    return super(MultiServerHttpIntegrationTestBase, self).getTestServerRootUri(False)

  def getAllTestServerRootUris(self):
    """See base class."""
    return super(MultiServerHttpIntegrationTestBase, self).getAllTestServerRootUris(False)


class HttpsIntegrationTestBase(IntegrationTestBase):
  """
  Base for https tests against the Nighthawk test server
  """

  def __init__(self, ip_version, server_config):
    """See base class."""
    super(HttpsIntegrationTestBase, self).__init__(ip_version, server_config)

  def getTestServerRootUri(self):
    """See base class."""
    return super(HttpsIntegrationTestBase, self).getTestServerRootUri(True)


class SniIntegrationTestBase(HttpsIntegrationTestBase):
  """
  Base for https/sni tests against the Nighthawk test server
  """

  def __init__(self, ip_version, server_config):
    super(SniIntegrationTestBase, self).__init__(ip_version, server_config)

  def getTestServerRootUri(self):
    """See base class."""
    return super(HttpsIntegrationTestBase, self).getTestServerRootUri(True)


class MultiServerHttpsIntegrationTestBase(IntegrationTestBase):
  """
  Base for https tests against multiple Nighthawk test servers
  """

  def __init__(self, ip_version, server_config, backend_count):
    super(MultiServerHttpsIntegrationTestBase, self).__init__(ip_version, server_config,
                                                              backend_count)

  def getTestServerRootUri(self):
    """See base class."""
    return super(MultiServerHttpsIntegrationTestBase, self).getTestServerRootUri(True)

  def getAllTestServerRootUris(self):
    """See base class."""
    return super(MultiServerHttpsIntegrationTestBase, self).getAllTestServerRootUris(True)


@pytest.fixture()
def server_config():
  yield "nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def http_test_server_fixture(request, server_config):
  f = HttpIntegrationTestBase(request.param, server_config)
  f.setUp()
  yield f
  f.tearDown()


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def https_test_server_fixture(request, server_config):
  f = HttpsIntegrationTestBase(request.param, server_config)
  f.setUp()
  yield f
  f.tearDown()


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def multi_http_test_server_fixture(request, server_config):
  f = MultiServerHttpIntegrationTestBase(request.param, server_config, backend_count=3)
  f.setUp()
  yield f
  f.tearDown()


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def multi_https_test_server_fixture(request, server_config):
  f = MultiServerHttpsIntegrationTestBase(request.param, server_config, backend_count=3)
  f.setUp()
  yield f
  f.tearDown()
