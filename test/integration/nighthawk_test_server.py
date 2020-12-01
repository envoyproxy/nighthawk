"""Contains the NighthawkTestServer class, which wraps the nighthawk_test_servern binary."""

import http.client
import json
import logging
import os
import socket
import subprocess
import sys
import random
import requests
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


class TestServerBase(object):
  """Base class for running a server in a separate process.

  Attributes:
    ip_version: IP version that the proxy should use when listening.
    server_ip: string containing the server ip that will be used to listen
    server_port: Integer, get the port used by the server to listen for traffic.
    docker_image: String, supplies a docker image for execution of the test server binary. Sourced from environment variable NH_DOCKER_IMAGE.
    tmpdir: String, indicates the location used to store outputs like logs.
  """

  def __init__(self, server_binary_path, config_template_path, server_ip, ip_version,
               server_binary_config_path_arg, parameters, tag):
    """Initialize a TestServerBase instance.

    Args:
        server_binary_path (str): specify the path to the server binary.
        config_template_path (str): specify the path to the test server configuration template.
        server_ip (str): Specify the ip address the test server should use to listen for traffic.
        ip_version (IPAddress): Specify the ip version the server should use to listen for traffic.
        server_binary_config_path_arg (str): Specify the name of the CLI argument the test server binary uses to accept a configuration path.
        parameters (dict): Supply to provide configuration template parameter replacement values.
        tag (str): Supply to get recognizeable output locations.
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
    self._prepareForExecution()

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
    logging.info("Test server popen() args: %s" % str.join(" ", args))
    self._server_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = self._server_process.communicate()
    logging.info("Process stdout: %s", stdout.decode("utf-8"))
    logging.info("Process stderr: %s", stderr.decode("utf-8"))

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
      time.sleep(1)
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
               parameters=dict(),
               tag=""):
    """Initialize a NighthawkTestServer instance.

    Args:
        server_binary_path (String): Path to the nighthawk test server binary.
        config_template_path (String): Path to the nighthawk test server configuration template.
        server_ip (String): Ip address for the server to use when listening.
        ip_version (IPVersion): IPVersion enum member indicating the ip version that the server should use when listening.
        parameters (dictionary, optional): Directionary with replacement values for substition purposes in the server configuration template. Defaults to dict().
        tag (str, optional): Tags. Supply this to get recognizeable output locations. Defaults to "".
    """
    super(NighthawkTestServer, self).__init__(server_binary_path, config_template_path, server_ip,
                                              ip_version, "--config-path", parameters, tag)

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
