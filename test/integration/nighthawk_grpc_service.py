import json
import logging
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
from string import Template

from common import IpVersion


class NighthawkGrpcService(object):
  """
    Base class for running a server in a separate process.
    """

  def __init__(self, server_binary_path, server_ip, ip_version):
    assert ip_version != IpVersion.UNKNOWN
    self.ip_version = ip_version
    self.server_binary_path = server_binary_path
    self.server_thread = threading.Thread(target=self.serverThreadRunner)
    self.server_process = None
    self.server_ip = server_ip
    self.socket_type = socket.AF_INET6 if ip_version == IpVersion.IPV6 else socket.AF_INET
    self.server_port = 0
    self.address_file = ""

  def serverThreadRunner(self):
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".tmp") as tmp:
      self.address_file = tmp.name
    args = [
        self.server_binary_path, "--listener-address-file", self.address_file, "--listen",
        "%s:0" % str(self.server_ip)
    ]
    logging.info("Nighthawk grpc service popen() args: [%s]" % args)
    self.server_process = subprocess.Popen(args)
    self.server_process.communicate()

  def waitUntilServerListening(self):
    tries = 10
    while tries > 0:
      contents = ""
      try:
        with open(self.address_file) as f:
          contents = f.read().strip()
      except (IOError):
        pass
      if contents != "":
        tmp = contents.split(":")
        assert (len(tmp) >= 2)
        self.server_port = int(tmp[len(tmp) - 1])
        return True
      time.sleep(0.5)
      tries -= 1

    logging.error("Timeout while waiting for server listener at %s:%s to accept connections.",
                  self.server_ip, self.server_port)
    return False

  def start(self):
    self.server_thread.daemon = True
    self.server_thread.start()
    return self.waitUntilServerListening()

  def stop(self):
    self.server_process.terminate()
    self.server_thread.join()
    return 0  #self.server_process.returncode
