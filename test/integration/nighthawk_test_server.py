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


class TestServerBase(object):
  """
    Base class for running a server in a separate process.
    """

  def __init__(self, server_binary_path, config_template_path, server_ip, ip_version,
               server_binary_config_path_arg, parameters, tag):
    assert ip_version != IpVersion.UNKNOWN
    self.ip_version = ip_version
    self.server_binary_path = server_binary_path
    self.config_template_path = config_template_path
    self.server_thread = threading.Thread(target=self.serverThreadRunner)
    self.server_process = None
    self.server_ip = server_ip
    self.socket_type = socket.AF_INET6 if ip_version == IpVersion.IPV6 else socket.AF_INET
    self.admin_address_path = ""
    self.parameterized_config_path = ""
    self.instance_id = str(random.randint(1, 1024 * 1024 * 1024))
    self.parameters = parameters
    self.server_binary_config_path_arg = server_binary_config_path_arg
    self.parameters["server_ip"] = self.server_ip
    self.docker_image = os.getenv("NH_NH_DOCKER_IMAGE", "")
    self.tmpdir = os.path.join(os.getenv("TMPDIR", "/tmp/nighthawk_benchmark/"), tag + "/")
    self.parameters["tmpdir"] = self.tmpdir
    self.parameters["tag"] = tag

    def substitute_yaml_values(obj, params):
      if isinstance(obj, dict):
        for k, v in obj.items():
          obj[k] = substitute_yaml_values(v, params)
      elif isinstance(obj, list):
        for i in range(len(obj)):
          obj[i] = substitute_yaml_values(obj[i], params)
      else:
        if isinstance(obj, str):
          # Inspect string values and substitute where applicable.
          INJECT_RUNFILE_MARKER = '@inject-runfile:'
          if obj[0] == '$':
            return Template(obj).substitute(params)
          elif obj.startswith(INJECT_RUNFILE_MARKER):
            r = runfiles.Create()
            with open(r.Rlocation(obj[len(INJECT_RUNFILE_MARKER):].strip()), 'r') as file:
              return file.read()
      return obj

    r = runfiles.Create()
    with open(r.Rlocation(self.config_template_path)) as f:
      data = yaml.load(f, Loader=yaml.FullLoader)
      data = substitute_yaml_values(data, self.parameters)

    Path(self.tmpdir).mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(
        mode="w", delete=False, suffix=".config.yaml", dir=self.tmpdir) as tmp:
      self.parameterized_config_path = tmp.name
      yaml.safe_dump(
          data,
          tmp,
          default_flow_style=False,
          explicit_start=True,
          allow_unicode=True,
          encoding='utf-8')

    with tempfile.NamedTemporaryFile(
        mode="w", delete=False, suffix=".adminport", dir=self.tmpdir) as tmp:
      self.admin_address_path = tmp.name

  def serverThreadRunner(self):
    args = []
    if self.docker_image != "":
      args = [
          "docker", "run", "--network=host", "--rm", "-v", "{t}:{t}".format(t=self.tmpdir),
          self.docker_image
      ]
    args = args + [
        self.server_binary_path, self.server_binary_config_path_arg, self.parameterized_config_path,
        "-l", "warning", "--base-id", self.instance_id, "--admin-address-path",
        self.admin_address_path
    ]
    logging.info("Test server popen() args: %s" % str.join(" ", args))
    self.server_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = self.server_process.communicate()
    logging.debug(stdout.decode("utf-8"))
    logging.debug(stderr.decode("utf-8"))

  def fetchJsonFromAdminInterface(self, path):
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

  def tryUpdateFromAdminInterface(self):
    with open(self.admin_address_path) as admin_address_file:
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
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip
    uri = "http://%s:%s%s" % (uri_host, self.admin_port, "/cpuprofiler?enable=y")
    r = requests.post(uri)
    logging.info("Enabled CPU profiling via %s: %s", uri, r.status_code == 200)
    return r.status_code == 200

  def waitUntilServerListening(self):
    # we allow 30 seconds for the server to have its listeners up.
    # (It seems that in sanitizer-enabled runs this can take a little while)
    timeout = time.time() + 10
    while time.time() < timeout:
      if self.tryUpdateFromAdminInterface():
        return True
      time.sleep(0.1)
    logging.error("Timeout in waitUntilServerListening()")
    return False

  def start(self):
    self.server_thread.daemon = True
    self.server_thread.start()
    return self.waitUntilServerListening()

  def stop(self):
    os.remove(self.admin_address_path)
    self.server_process.terminate()
    self.server_thread.join()
    return self.server_process.returncode


class NighthawkTestServer(TestServerBase):
  """
    Will run the Nighthawk test server in a separate process. Passes in the right cli-arg to point it to its
    configuration. For, say, NGINX this would be '-c' instead.
    """

  def __init__(self,
               server_binary_path,
               config_template_path,
               server_ip,
               ip_version,
               parameters=dict(),
               tag=""):
    super(NighthawkTestServer, self).__init__(server_binary_path, config_template_path, server_ip,
                                              ip_version, "--config-path", parameters, tag)
