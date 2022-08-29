"""Contains unit tests for subprocess_mixin.py."""

import pytest

from test.integration.subprocess_mixin import SubprocessMixin


class TestSubprocessMixin(SubprocessMixin):
  """Helper class for testing the SubprocessMixin."""

  def __init__(self, args):
    """Create the TestSubprocessMixin."""
    super().__init__()
    self.args = args
    self.stdout = b''
    self.stderr = b''

  def _argsForSubprocess(self) -> list[str]:
    return self.args

  def _serverThreadRunner(self):
    self.stdout, self.stderr = super()._serverThreadRunner()


def test_subprocess_captures_stdout():
  """Test the subprocess captures stdout."""
  child_process = TestSubprocessMixin(['echo', 'stdout'])
  child_process.launchSubprocess()
  child_process.waitUntilSubprocessLaunched()
  child_process.waitForSubprocessNotRunning()
  assert b'stdout' in child_process.stdout


def test_subprocess_captures_stderr():
  """Test the subprocess captures stderr."""
  child_process = TestSubprocessMixin(['logger', '--no-act', '-s', 'stderr'])
  child_process.launchSubprocess()
  child_process.waitUntilSubprocessLaunched()
  child_process.waitForSubprocessNotRunning()
  assert child_process.stderr != b''


def test_subprocess_stop():
  """Test the subprocess can be stopped."""
  child_process = TestSubprocessMixin(['sleep', '120'])
  child_process.launchSubprocess()
  child_process.waitUntilSubprocessLaunched()
  ret_code = child_process.stopSubprocess()
  # Non-zero exit is expected as the subprocess should be killed.
  assert ret_code != 0


if __name__ == '__main__':
  raise SystemExit(pytest.main([__file__, '--assert=plain']))
