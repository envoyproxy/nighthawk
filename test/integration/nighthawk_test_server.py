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
from string import Template

from common import IpVersion, NighthawkException


class TestServerBase(object):
  """
    Base class for running a server in a separate process.
    """

  def __init__(self, server_binary_path, config_template_path, server_ip, ip_version,
               server_binary_config_path_arg, parameters):
    assert ip_version != IpVersion.UNKNOWN
    self.ip_version = ip_version
    self.server_binary_path = server_binary_path
    self.config_template_path = config_template_path
    self.server_thread = threading.Thread(target=self.serverThreadRunner)
    self.server_process = None
    self.server_ip = server_ip
    self.socket_type = socket.AF_INET6 if ip_version == IpVersion.IPV6 else socket.AF_INET
    self.server_port = -1
    self.admin_port = -1
    self.admin_address_path = ""
    self.parameterized_config_path = ""
    self.instance_id = str(random.randint(1, 1024 * 1024 * 1024))
    self.parameters = parameters
    self.server_binary_config_path_arg = server_binary_config_path_arg

    self.parameters["server_ip"] = self.server_ip
    with open(self.config_template_path) as f:
      config = Template(f.read())
      config = config.substitute(self.parameters)
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".yaml") as tmp:
      self.parameterized_config_path = tmp.name
      tmp.write(config)
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".adminpath") as tmp:
      self.admin_address_path = tmp.name

  def serverThreadRunner(self):
    args = [
        self.server_binary_path, self.server_binary_config_path_arg, self.parameterized_config_path,
        "-l", "error", "--base-id", self.instance_id, "--admin-address-path",
        self.admin_address_path
    ]
    logging.info("Test server popen() args: [%s]" % args)
    self.server_process = subprocess.Popen(args)
    self.server_process.communicate()

  def fetchJsonFromAdminInterface(self, path):
    uri_host = self.server_ip
    if self.ip_version == IpVersion.IPV6:
      uri_host = "[%s]" % self.server_ip
    uri = "http://%s:%s%s" % (uri_host, self.admin_port, path)
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
    except ConnectionError:
      return False

  def waitUntilServerListening(self):
    timeout = time.time() + 5
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
               parameters=dict()):
    super(NighthawkTestServer, self).__init__(server_binary_path, config_template_path, server_ip,
                                              ip_version, "--config-path", parameters)
