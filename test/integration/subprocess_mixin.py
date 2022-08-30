"""Mixin for managing child subprocess with an additional thread."""

from threading import Thread, Condition
import subprocess
import logging
from abc import ABC, abstractmethod


class SubprocessMixin(ABC):
  """Mixin used to manage launching subprocess using a separate Python thread.

  See test_subprocess_captures_stdout and test_subprocess_stop to see usage
  of the mixin.
  """

  def __init__(self):
    """Create SubprocessMixin."""
    self._server_thread = Thread(target=self._serverThreadRunner)
    self._server_process = None
    self._has_launched = False
    self._has_launched_cv = Condition()

  @abstractmethod
  def _argsForSubprocess(self) -> list[str]:
    """Return the args to launch the subprocess."""
    pass

  def _serverThreadRunner(self):
    """Routine executed by the python thread that launches the child subprocess.

    Derived classes may wish to extend this to use the stdout, stderr of
    the child process.
    """
    args = self._argsForSubprocess()
    logging.info("Test server popen() args: %s" % str.join(" ", args))
    self._server_process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with self._has_launched_cv:
      self._has_launched = True
      self._has_launched_cv.notify_all()
    stdout, stderr = self._server_process.communicate()
    logging.info("Process stdout: %s", stdout.decode("utf-8"))
    logging.info("Process stderr: %s", stderr.decode("utf-8"))
    return stdout, stderr

  def launchSubprocess(self):
    """Start the subprocess."""
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
    """Wait for the subprocess to not be running assuming it exits."""
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
