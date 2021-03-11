"""Contains the NighthawkTestServer class, which wraps the nighthawk_test_servern binary."""

import collections
import http.client
import json
import logging
import os
import random
import re
import requests
import socket
import subprocess
import sys
import tempfile
import threading
import time
import yaml
from string import Template
from pathlib import Path
from rules_python.python.runfiles import runfiles

from test.integration.common import IpVersion, NighthawkException


def _substitute_yaml_values(runfiles_instance, obj, params):
  if isinstance(obj, dict):
    for k, v in obj.items():
      obj[k] = _substitute_yaml_values(runfiles_instance, v, params)
  elif isinstance(obj, list):
    for i in range(len(obj)):
      obj[i] = _substitute_yaml_values(runfiles_instance, obj[i], params)
  else:
    if isinstance(obj, str):
      # Inspect string values and substitute where applicable.
      INJECT_RUNFILE_MARKER = '@inject-runfile:'
      if obj[0] == '$':
        return Template(obj).substitute(params)
      elif obj.startswith(INJECT_RUNFILE_MARKER):
        with open(runfiles_instance.Rlocation(obj[len(INJECT_RUNFILE_MARKER):].strip()),
                  'r') as file:
          return file.read()
  return obj


class _TestCaseWarnErrorIgnoreList(
    collections.namedtuple("_TestCaseWarnErrorIgnoreList", "test_case_regexp ignore_list")):
  """Maps test case names to messages that should be ignored in the test server logs.

  If the name of the currently executing test case matches the test_case_regexp,
  any messages logged by the test server as either a WARNING or an ERROR
  will be checked against the ignore_list. If the logged messages contain any of
  the messages in the ignore list as a substring, they will be ignored.
  Any unmatched messages of either a WARNING or an ERROR severity will fail the
  test case.

  Attributes:
    test_case_regexp: A compiled regular expression as returned by re.compile(),
      the regexp that will be used to match test case names.
    ignore_list: A tuple of strings, messages to ignore for matching test cases.
  """


# A list of _TestCaseWarnErrorIgnoreList instances, message pieces that should
# be ignored even if logged by the test server at a WARNING or an ERROR
# severity.
#
# If multiple test_case_regexp entries match the current test case name, all the
# corresponding ignore lists will be used.
_TEST_SERVER_WARN_ERROR_IGNORE_LIST = frozenset([
    # This test case purposefully uses the deprecated Envoy v2 API which emits
    # the following warnings.
    _TestCaseWarnErrorIgnoreList(
        re.compile('test_nighthawk_test_server_envoy_deprecated_v2_api'),
        (
            "Configuration does not parse cleanly as v3. v2 configuration is deprecated",
            "Deprecated field: type envoy.api.v2.listener.Filter",
            "Deprecated field: type envoy.config.filter.network.http_connection_manager.v2.HttpFilter",
            "Using deprecated extension name 'envoy.http_connection_manager'",
            "Using deprecated extension name 'envoy.router'",
        ),
    ),

    # A catch-all that applies to all remaining test cases.
    _TestCaseWarnErrorIgnoreList(
        re.compile('.*'),
        (
            # TODO(#582): Identify these and file issues or add explanation as necessary.
            "Unable to use runtime singleton for feature envoy.http.headermap.lazy_map_min_size",
            "Unable to use runtime singleton for feature envoy.reloadable_features.header_map_correctly_coalesce_cookies",
            "Using deprecated extension name 'envoy.listener.tls_inspector' for 'envoy.filters.listener.tls_inspector'.",
            "there is no configured limit to the number of allowed active connections. Set a limit via the runtime key overload.global_downstream_max_connections",

            # A few of our filters use the same typed configuration, specifically
            # 'test-server', 'time-tracking' and 'dynamic-delay'.
            # For now this is by design.
            "Double registration for type: 'nighthawk.server.ResponseOptions'",

            # Logged for normal termination, not really a warning.
            "caught ENVOY_SIGTERM",
        ),
    ),
])


class TestServerBase(object):
  """Base class for running a server in a separate process.

  Attributes:
    ip_version: IP version that the proxy should use when listening.
    server_ip: string containing the server ip that will be used to listen
    server_port: Integer, get the port used by the server to listen for traffic.
    docker_image: String, supplies a docker image for execution of the test server binary. Sourced from environment variable NH_DOCKER_IMAGE.
    tmpdir: String, indicates the location used to store outputs like logs.
  """

  def __init__(self,
               server_binary_path,
               config_template_path,
               server_ip,
               ip_version,
               request,
               server_binary_config_path_arg,
               parameters,
               tag,
               bootstrap_version_arg=None):
    """Initialize a TestServerBase instance.

    Args:
        server_binary_path (str): specify the path to the server binary.
        config_template_path (str): specify the path to the test server configuration template.
        server_ip (str): Specify the ip address the test server should use to listen for traffic.
        ip_version (IPAddress): Specify the ip version the server should use to listen for traffic.
        request: The pytest `request` fixture used to determin information about the currently executed test.
        server_binary_config_path_arg (str): Specify the name of the CLI argument the test server binary uses to accept a configuration path.
        parameters (dict): Supply to provide configuration template parameter replacement values.
        tag (str): Supply to get recognizeable output locations.
        bootstrap_version_arg (int, optional): specify a bootstrap cli argument value for the test server binary.
    """
    assert ip_version != IpVersion.UNKNOWN
    self.ip_version = ip_version
    self.server_ip = server_ip
    self.server_port = -1
    self.docker_image = os.getenv("NH_DOCKER_IMAGE", "")
    self.tmpdir = os.path.join(os.getenv("TMPDIR", "/tmp/nighthawk_benchmark/"), tag + "/")
    self._server_binary_path = server_binary_path
    self._config_template_path = config_template_path
    self._parameters = dict(parameters)
    self._parameters["server_ip"] = self.server_ip
    self._parameters["tmpdir"] = self.tmpdir
    self._parameters["tag"] = tag
    self._server_process = None
    self._server_thread = threading.Thread(target=self._serverThreadRunner)
    self._admin_address_path = ""
    self._parameterized_config_path = ""
    self._instance_id = str(random.randint(1, 1024 * 1024 * 1024))
    self._server_binary_config_path_arg = server_binary_config_path_arg
    self._bootstrap_version_arg = bootstrap_version_arg
    self._prepareForExecution()
    self._request = request

  def _prepareForExecution(self):
    runfiles_instance = runfiles.Create()
    with open(runfiles_instance.Rlocation(self._config_template_path)) as f:
      data = yaml.load(f, Loader=yaml.FullLoader)
      data = _substitute_yaml_values(runfiles_instance, data, self._parameters)

    Path(self.tmpdir).mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".config.yaml",
                                     dir=self.tmpdir) as tmp:
      self._parameterized_config_path = tmp.name
      yaml.safe_dump(data,
                     tmp,
                     default_flow_style=False,
                     explicit_start=True,
                     allow_unicode=True,
                     encoding='utf-8')

    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".adminport",
                                     dir=self.tmpdir) as tmp:
      self._admin_address_path = tmp.name

  def _serverThreadRunner(self):
    args = []
    if self.docker_image != "":
      # TODO(#383): As of https://github.com/envoyproxy/envoy/commit/e8a2d1e24dc9a0da5273442204ec3cdfad1e7ca8
      # we need to have ENVOY_UID=0 in the environment, or this will break on docker runs, as Envoy
      # will not be able to read the configuration files we stub here in docker runs.
      args = [
          "docker", "run", "--network=host", "--rm", "-v", "{t}:{t}".format(t=self.tmpdir), "-e",
          "ENVOY_UID=0", self.docker_image
      ]
    args = args + [
        self._server_binary_path, self._server_binary_config_path_arg,
        self._parameterized_config_path, "-l", "debug", "--base-id", self._instance_id,
        "--admin-address-path", self._admin_address_path, "--concurrency", "1"
    ]
    if self._bootstrap_version_arg is not None:
      args = args + ["--bootstrap-version", str(self._bootstrap_version_arg)]

    logging.info("Test server popen() args: %s" % str.join(" ", args))
    self._server_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = self._server_process.communicate()
    logging.info("Process stdout: %s", stdout.decode("utf-8"))
    logging.info("Process stderr: %s", stderr.decode("utf-8"))
    warnings, errors = _extractWarningsAndErrors(stdout.decode() + stderr.decode(),
                                                 self._request.node.name,
                                                 _TEST_SERVER_WARN_ERROR_IGNORE_LIST)
    if warnings:
      [logging.warn("Process logged a warning: %s", w) for w in warnings]
    if errors:
      [logging.error("Process logged an error: %s", e) for e in errors]

  def fetchJsonFromAdminInterface(self, path):
    """Fetch and parse json from the admin interface.

    Args:
        path (str): Request uri path and query to use when fetching. E.g. "/stats?format=json"

    Raises:
        NighthawkException: Raised when the fetch resulted in any http status code other then 200.

    Returns:
        json: Parsed json object.
    """
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip
    uri = "http://%s:%s%s" % (uri_host, self.admin_port, path)
    logging.info("Fetch listeners via %s" % uri)
    r = requests.get(uri)
    if r.status_code != 200:
      raise NighthawkException("Bad status code wile fetching json from admin interface: %s",
                               r.status_code)
    return r.json()

  def _tryUpdateFromAdminInterface(self):
    with open(self._admin_address_path) as admin_address_file:
      admin_address = admin_address_file.read()
    tmp = admin_address.split(":")
    # we expect at least two elements (host:port). This might still be an empty file
    # if the test server is still working to boot up.
    if len(tmp) < 2:
      return False
    self.admin_port = tmp[len(tmp) - 1]
    try:
      listeners = self.fetchJsonFromAdminInterface("/listeners?format=json")
      # Right now we assume there's only a single listener
      self.server_port = listeners["listener_statuses"][0]["local_address"]["socket_address"][
          "port_value"]
      return True
    except requests.exceptions.ConnectionError:
      return False

  def enableCpuProfiler(self):
    """Enable the built-in cpu profiler of the test server.

    Returns:
        Bool: True iff the cpu profiler was succesfully enabled.
    """
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip
    uri = "http://%s:%s%s" % (uri_host, self.admin_port, "/cpuprofiler?enable=y")
    r = requests.post(uri)
    logging.info("Enabled CPU profiling via %s: %s", uri, r.status_code == 200)
    return r.status_code == 200

  def _waitUntilServerListening(self):
    # we allow some time for the server to have its listeners up.
    # (It seems that in sanitizer-enabled runs this can take a little while)
    timeout = time.time() + 60
    while time.time() < timeout:
      if self._tryUpdateFromAdminInterface():
        return True
      time.sleep(0.1)
    logging.error("Timeout in _waitUntilServerListening()")
    return False

  def start(self):
    """Start the server.

    Returns:
        Bool: True iff the server started successfully.
    """
    self._server_thread.daemon = True
    self._server_thread.start()
    return self._waitUntilServerListening()

  def stop(self):
    """Stop the server.

    Returns:
        Int: exit code of the server process.
    """
    os.remove(self._admin_address_path)
    self._server_process.terminate()
    self._server_thread.join()
    return self._server_process.returncode


class NighthawkTestServer(TestServerBase):
  """Run the Nighthawk test server in a separate process.

  Passes in the right cli-arg to point it to its
  configuration. For, say, NGINX this would be '-c' instead.
  """

  def __init__(self,
               server_binary_path,
               config_template_path,
               server_ip,
               ip_version,
               request,
               parameters=dict(),
               tag="",
               bootstrap_version_arg=None):
    """Initialize a NighthawkTestServer instance.

    Args:
        server_binary_path (String): Path to the nighthawk test server binary.
        config_template_path (String): Path to the nighthawk test server configuration template.
        server_ip (String): Ip address for the server to use when listening.
        ip_version (IPVersion): IPVersion enum member indicating the ip version that the server should use when listening.
        request: The pytest `request` fixture used to determin information about the currently executed test.
        parameters (dictionary, optional): Directionary with replacement values for substition purposes in the server configuration template. Defaults to dict().
        tag (str, optional): Tags. Supply this to get recognizeable output locations. Defaults to "".
        bootstrap_version_arg (String, optional): Specify a cli argument value for --bootstrap-version when running the server.
    """
    super(NighthawkTestServer, self).__init__(server_binary_path,
                                              config_template_path,
                                              server_ip,
                                              ip_version,
                                              request,
                                              "--config-path",
                                              parameters,
                                              tag,
                                              bootstrap_version_arg=bootstrap_version_arg)

  def getCliVersionString(self):
    """Get the version string as written to the output by the CLI."""
    args = []
    if self.docker_image != "":
      args = ["docker", "run", "--rm", self.docker_image]
    args = args + [self._server_binary_path, "--base-id", self._instance_id, "--version"]

    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    assert process.wait() == 0
    return stdout.decode("utf-8").strip()


def _matchesAnyIgnoreListEntry(line, test_case_name, ignore_list):
  """Determine if the line matches any of the ignore list entries for this test case.

  Args:
    line: A string, the logged line.
    test_case_name: A string, name of the currently executed test case.
    ignore_list: A list of _TestCaseWarnErrorIgnoreList instances, the ignore
      lists to match against.

  Returns:
    A boolean, True if the logged line matches any of the ignore list entries,
    False otherwise.
  """
  for test_case_ignore_list in ignore_list:
    if not test_case_ignore_list.test_case_regexp.match(test_case_name):
      continue
    for ignore_message in test_case_ignore_list.ignore_list:
      if ignore_message in line:
        return True
  return False


def _extractWarningsAndErrors(process_output, test_case_name, ignore_list):
  """Extract warnings and errors from the process_output.

  Args:
    process_output: A string, the stdout or stderr after running a process.
    test_case_name: A string, the name of the current test case.
    ignore_list: A list of _TestCaseWarnErrorIgnoreList instances, the message
      pieces to ignore. If a message that was logged either at a WARNING or at
      an ERROR severity contains one of these message pieces and should be
      ignored for the current test case, it will be excluded from the return
      values.

  Returns:
    A tuple of two lists of strings, the first list contains the warnings found
    in the process_output and the second list contains the errors found in the
    process_output.
  """
  warnings = []
  errors = []
  for line in process_output.split('\n'):
    # Optimization - no need to examine lines that aren't errors or warnings.
    if "[warning]" not in line and "[error]" not in line:
      continue

    if _matchesAnyIgnoreListEntry(line, test_case_name, ignore_list):
      continue

    if "[warning]" in line:
      warnings.append(line)
    elif "[error]" in line:
      errors.append(line)
  return warnings, errors
