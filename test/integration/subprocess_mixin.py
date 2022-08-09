from threading import Thread, Condition
import subprocess
import logging
from abc import ABC, abstractmethod


class SubprocessMixin(ABC):
  """Mixin used to manage launching subprocess using a separate Python thread."""

  def __init__(self):
    self._server_thread = Thread(target=self._serverThreadRunner)
    self._server_process = None
    self._has_launched = False
    self._has_launched_cv = Condition()
    self.stdout = b''
    self.stderr = b''

  @abstractmethod
  def _argsForSubprocess(self) -> list[str]:
    """Returns the args to launch the subprocess."""
    pass

  def _serverThreadRunner(self):
    args = self._argsForSubprocess()
    logging.info("Test server popen() args: %s" % str.join(" ", args))
    self._server_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with self._has_launched_cv:
      self._has_launched = True
      self._has_launched_cv.notify_all()
    self.stdout, self.stderr = self._server_process.communicate()
    logging.info("Process stdout: %s", self.stdout.decode("utf-8"))
    logging.info("Process stderr: %s", self.stderr.decode("utf-8"))
    return self.stdout, self.stderr

  def launchSubprocess(self):
    """Start the subprocess. """
    self._server_thread.daemon = True
    self._server_thread.start()

  def waitUntilSubprocessLaunched(self):
    """Blocks until the subprocess has launched at least once."""

    def hasLaunched():
      return self._has_launched

    with self._has_launched_cv:
      self._has_launched_cv.wait_for(hasLaunched)
      assert self._has_launched

  def waitForSubprocessNotRunning(self):
    """Waits for the subprocess to not be running assuming it exits."""
    if not self._has_launched or not self._server_thread.is_alive():
      return
    self._server_thread.join()

    def hasLaunched():
      return self._has_launched

    with self._has_launched_cv:
      self._has_launched_cv.wait_for(hasLaunched)
      assert self._has_launched

  def stopSubprocess(self) -> int:
    """Stop the subprocess.

    Returns:
        Int: exit code of the server process.
    """
    self._server_process.terminate()
    self._server_thread.join()
    return self._server_process.returncode
