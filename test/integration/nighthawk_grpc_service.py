import logging
import socket
import subprocess
import tempfile
import threading
import time

from common import IpVersion


# TODO(oschaaf): unify some of this code with the test server wrapper.
class NighthawkGrpcService(object):
  """
  Class for running the Nighthawk gRPC service in a separate process.
  Usage:
    grpc_service = NighthawkGrpcService("/path/to/nighthawk_service"), "127.0.0.1", IpVersion.IPV4)
    if grpc_service.start():
      .... You can talk to the Nighthawk gRPC service at the 127.0.0.1:grpc_service.server_port ...

  Attributes:
  server_ip: IP address used by the gRPC service to listen. 
  server_port: An integer, indicates the port used by the gRPC service to listen. 0 means that the server is not listening.
  """

  def __init__(self, server_binary_path, server_ip, ip_version):
    """Initializes Nighthawk gRPC service.

    Args:
    server_binary_path: A string, indicates where the nighthawk gRPC service binary resides
    server_ip: IP address, indicates which ip address should be used by the gRPC service listener.
    ip_version: IP Version, indicates if IPv4 or IPv6 should be used.
    ...
    """
    assert ip_version != IpVersion.UNKNOWN
    self.server_port = 0
    self.server_ip = server_ip
    self._server_process = None
    self._ip_version = ip_version
    self._server_binary_path = server_binary_path
    self._socket_type = socket.AF_INET6 if ip_version == IpVersion.IPV6 else socket.AF_INET
    self._server_thread = threading.Thread(target=self._serverThreadRunner)
    self._address_file = None

  def _serverThreadRunner(self):
    with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".tmp") as tmp:
      self._address_file = tmp.name
      args = [
          self._server_binary_path, "--listener-address-file", self._address_file, "--listen",
          "%s:0" % str(self.server_ip)
      ]
      logging.info("Nighthawk grpc service popen() args: [%s]" % args)
      self._server_process = subprocess.Popen(args)
      self._server_process.communicate()
      self._address_file = None

  def _waitUntilServerListening(self):
    tries = 30
    while tries > 0:
      contents = ""
      if not self._address_file is None:
        try:
          with open(self._address_file) as f:
            contents = f.read().strip()
        except IOError:
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
    """
    Starts the Nighthawk gRPC service. Returns True upon success, after which the server_port attribute
    can be queried to get the listening port.
    """

    self._server_thread.daemon = True
    self._server_thread.start()
    return self._waitUntilServerListening()

  def stop(self):
    """
    Signals the Nighthawk gRPC service to stop, waits for its termination, and returns the exit code of the associated process.
    """

    self._server_process.terminate()
    self._server_thread.join()
    self.server_port = 0
    return self._server_process.returncode
