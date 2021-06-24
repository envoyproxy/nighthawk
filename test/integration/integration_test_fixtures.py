"""Base classes for Nighthawk integration tests."""

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
  """Determine the IP version(s) for test execution from the environment.

  Raises:
      NighthawkException: raised when no ip version could be determined, or
      an invalid one was encountered.

  Returns:
      A list of test.integration.common.IpVersion with ip versions obtained from the
      ENVOY_IP_TEST_VERSIONS environment variable.
  """
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
  """Base class for integration test fixtures.

  IntegrationTestBase facilitates testing against the Nighthawk test server, by determining a free port,
  and starting it up in a separate process in setUp().

  Support for multiple test servers has been added in a way that minimizes impact to existing tests.
  self.test_server always points to the first test server, and methods assuming a single backend such
  as getTestServerRootUri were left intact. self._test_servers contains all test servers, including the
  first. Methods such as getTestServerRootUris that are aware of multiple test servers will also
  work when there is only one test server.

  This class will be refactored (https://github.com/envoyproxy/nighthawk/issues/258).

  Attributes:
    ip_version: IP version that the proxy should use when listening.
    server_ip: string containing the server ip that will be used to listen
    tag: String. Supply this to get recognizeable output locations.
    parameters: Dictionary. Supply this to provide template parameter replacement values.
    grpc_service: NighthawkGrpcService instance or None. Set by startNighthawkGrpcService().
    test_server: NighthawkTestServer instance, set during setUp().
    nighthawk_client_path: String, path to the nighthawk_client binary.
    request: The pytest `request` test fixture used to determine information
      about the currently executing test case.
  """

  def __init__(self, request, server_config, backend_count=1):
    """Initialize the IntegrationTestBase instance.

    Args:
      ip_version: a single IP mode that this instance will test: IpVersion.IPV4 or IpVersion.IPV6
      request: The pytest `request` test fixture used to determine information
        about the currently executing test case.
      server_config: path to the server configuration
      backend_count: number of Nighthawk Test Server backends to run, to allow testing MultiTarget mode
    """
    super(IntegrationTestBase, self).__init__()
    self.request = request
    self.ip_version = request.param
    assert self.ip_version != IpVersion.UNKNOWN
    self.server_ip = "::" if self.ip_version == IpVersion.IPV6 else "0.0.0.0"
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
    self._socket_type = socket.AF_INET6 if self.ip_version == IpVersion.IPV6 else socket.AF_INET
    self._test_servers = []
    self._backend_count = backend_count
    self._test_id = ""

  # TODO(oschaaf): For the NH test server, add a way to let it determine a port by itself and pull that
  # out.
  def getFreeListenerPortForAddress(self, address):
    """Determine a free port and returns that.

    Theoretically it is possible that another process
    will steal the port before our caller is able to leverage it, but we take that chance.
    The upside is that we can push the port upon the server we are about to start through configuration
    which is compatible accross servers.
    """
    with socket.socket(self._socket_type, socket.SOCK_STREAM) as sock:
      sock.bind((address, 0))
      port = sock.getsockname()[1]
    return port

  def setUp(self):
    """Perform sanity checks and start up the server.

    Upon exit the server is ready to accept connections.
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

  def tearDown(self, caplog):
    """Stop the server.

    Fails the test if any warnings or errors were logged.

    Args:
      caplog: The pytest `caplog` test fixture used to examine logged messages.
    """
    if self.grpc_service is not None:
      if self.grpc_service.stop() != 0:
        pytest.fail(
            "the Nighthawk GRPC service reported a non-zero exit code when stopped, log lines:\n{}".
            format('\n'.join(self.grpc_service.log_lines)))

    any_failed = False
    for test_server in self._test_servers:
      if test_server.stop() != 0:
        any_failed = True
    assert (not any_failed)

    warnings_and_errors = []
    for when in ("setup", "call", "teardown"):
      for record in caplog.get_records(when):
        if record.levelno not in (logging.WARNING, logging.ERROR):
          continue
        warnings_and_errors.append(record.message)
    if warnings_and_errors:
      pytest.fail("warnings or errors encountered during testing:\n{}".format(warnings_and_errors))

  def _tryStartTestServers(self):
    for i in range(self._backend_count):
      test_server = NighthawkTestServer(self._nighthawk_test_server_path,
                                        self._nighthawk_test_config_path,
                                        self.server_ip,
                                        self.ip_version,
                                        self.request,
                                        parameters=self.parameters,
                                        tag=self.tag)
      if not test_server.start():
        return False
      self._test_servers.append(test_server)
      if i == 0:
        self.test_server = test_server
    return True

  def getGlobalResults(self, parsed_json):
    """Find the global/aggregated result in the json output."""
    global_result = [x for x in parsed_json["results"] if x["name"] == "global"]
    assert (len(global_result) == 1)
    return global_result[0]

  def getNighthawkCounterMapFromJson(self, parsed_json):
    """Get the counters from the json indexed by name."""
    return {
        counter["name"]: int(counter["value"])
        for counter in self.getGlobalResults(parsed_json)["counters"]
    }

  def getNighthawkGlobalHistogramsbyIdFromJson(self, parsed_json):
    """Get the global histograms from the json indexed by id."""
    return {
        statistic["id"]: statistic for statistic in self.getGlobalResults(parsed_json)["statistics"]
    }

  def getTestServerRootUri(self, https=False):
    """Get the http://host:port/ that can be used to query the server we started in setUp()."""
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip

    uri = "%s://%s:%s/" % ("https" if https else "http", uri_host, self.test_server.server_port)
    return uri

  def getAllTestServerRootUris(self, https=False):
    """Get the list of http://host:port/ that can be used to query the servers we started in setUp()."""
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip

    return [
        "%s://%s:%s/" % ("https" if https else "http", uri_host, test_server.server_port)
        for test_server in self._test_servers
    ]

  def getTestServerStatisticsJson(self):
    """Grab a statistics snapshot from the test server."""
    return self.test_server.fetchJsonFromAdminInterface("/stats?format=json")

  def getAllTestServerStatisticsJsons(self):
    """Grab a statistics snapshot from multiple test servers."""
    return [
        test_server.fetchJsonFromAdminInterface("/stats?format=json")
        for test_server in self._test_servers
    ]

  def getServerStatFromJson(self, server_stats_json, name):
    """Extract one statistic from a single json snapshot."""
    counters = server_stats_json["stats"]
    for counter in counters:
      if counter["name"] == name:
        return int(counter["value"])
    return None

  def runNighthawkClient(self, args, expect_failure=False, timeout=30, as_json=True):
    """Run Nighthawk against the test server.

    Returns a string containing json-formatted result plus logs.
    If the timeout is exceeded an exception will be raised.
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
    logging.info("Nighthawk client stdout: [%s]" % output)
    if logs:
      logging.info("Nighthawk client stderr: [%s]" % logs)
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
    # We suppress declared but not used warnings below, as these may produce helpful
    # in test failures (via pytests introspection and logging).
    logs = stderr.decode('utf-8')  # noqa(F841)
    output = stdout.decode('utf-8')  # noqa(F841)
    assert (client_process.returncode == 0)
    return stdout.decode('utf-8')

  def startNighthawkGrpcService(self, service_name="traffic-generator-service"):
    """Start the Nighthawk gRPC service.

    Args:
        service_name (String, optional): Service type to start. Defaults to "traffic-generator-service".
    """
    host = self.server_ip if self.ip_version == IpVersion.IPV4 else "[%s]" % self.server_ip
    self.grpc_service = NighthawkGrpcService(self._nighthawk_service_path, host, self.ip_version,
                                             service_name)
    assert (self.grpc_service.start())


class HttpIntegrationTestBase(IntegrationTestBase):
  """Base for running plain http tests against the Nighthawk test server.

  NOTE: any script that consumes derivations of this, needs to also explicitly
  import server_config, to avoid errors caused by the server_config not being found
  by pytest.
  """

  def __init__(self, request, server_config):
    """See base class."""
    super(HttpIntegrationTestBase, self).__init__(request, server_config)

  def getTestServerRootUri(self):
    """See base class."""
    return super(HttpIntegrationTestBase, self).getTestServerRootUri(False)


class MultiServerHttpIntegrationTestBase(IntegrationTestBase):
  """Base for running plain http tests against multiple Nighthawk test servers."""

  def __init__(self, request, server_config, backend_count):
    """See base class."""
    super(MultiServerHttpIntegrationTestBase, self).__init__(request, server_config, backend_count)

  def getTestServerRootUri(self):
    """See base class."""
    return super(MultiServerHttpIntegrationTestBase, self).getTestServerRootUri(False)

  def getAllTestServerRootUris(self):
    """See base class."""
    return super(MultiServerHttpIntegrationTestBase, self).getAllTestServerRootUris(False)


class HttpsIntegrationTestBase(IntegrationTestBase):
  """Base for https tests against the Nighthawk test server."""

  def __init__(self, request, server_config):
    """See base class."""
    super(HttpsIntegrationTestBase, self).__init__(request, server_config)

  def getTestServerRootUri(self):
    """See base class."""
    return super(HttpsIntegrationTestBase, self).getTestServerRootUri(True)


class SniIntegrationTestBase(HttpsIntegrationTestBase):
  """Base for https/sni tests against the Nighthawk test server."""

  def __init__(self, request, server_config):
    """See base class."""
    super(SniIntegrationTestBase, self).__init__(request, server_config)

  def getTestServerRootUri(self):
    """See base class."""
    return super(HttpsIntegrationTestBase, self).getTestServerRootUri(True)


class MultiServerHttpsIntegrationTestBase(IntegrationTestBase):
  """Base for https tests against multiple Nighthawk test servers."""

  def __init__(self, request, server_config, backend_count):
    """See base class."""
    super(MultiServerHttpsIntegrationTestBase, self).__init__(request, server_config, backend_count)

  def getTestServerRootUri(self):
    """See base class."""
    return super(MultiServerHttpsIntegrationTestBase, self).getTestServerRootUri(True)

  def getAllTestServerRootUris(self):
    """See base class."""
    return super(MultiServerHttpsIntegrationTestBase, self).getAllTestServerRootUris(True)


@pytest.fixture()
def server_config():
  """Fixture which yields the path to the stock server configuration.

  Yields:
      String: Path to the stock server configuration.
  """
  yield "nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def http_test_server_fixture(request, server_config, caplog):
  """Fixture for setting up a test environment with the stock http server configuration.

  Yields:
      HttpIntegrationTestBase: A fully set up instance. Tear down will happen automatically.
  """
  f = HttpIntegrationTestBase(request, server_config)
  f.setUp()
  yield f
  f.tearDown(caplog)


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def https_test_server_fixture(request, server_config, caplog):
  """Fixture for setting up a test environment with the stock https server configuration.

  Yields:
      HttpsIntegrationTestBase: A fully set up instance. Tear down will happen automatically.
  """
  f = HttpsIntegrationTestBase(request, server_config)
  f.setUp()
  yield f
  f.tearDown(caplog)


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def multi_http_test_server_fixture(request, server_config, caplog):
  """Fixture for setting up a test environment with multiple servers, using the stock http server configuration.

  Yields:
      MultiServerHttpIntegrationTestBase: A fully set up instance. Tear down will happen automatically.
  """
  f = MultiServerHttpIntegrationTestBase(request, server_config, backend_count=3)
  f.setUp()
  yield f
  f.tearDown(caplog)


@pytest.fixture(params=determineIpVersionsFromEnvironment())
def multi_https_test_server_fixture(request, server_config, caplog):
  """Fixture for setting up a test environment with multiple servers, using the stock https server configuration.

  Yields:
      MultiServerHttpsIntegrationTestBase: A fully set up instance. Tear down will happen automatically.
  """
  f = MultiServerHttpsIntegrationTestBase(request, server_config, backend_count=3)
  f.setUp()
  yield f
  f.tearDown(caplog)
