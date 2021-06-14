"""Packages utility methods for tests."""

import os
import subprocess


class Error(Exception):
  """Raised on errors in this module."""


def isSanitizerRun():
  """Determine if the current execution is a tsan/asan/ubsan run.

  Returns:
      bool: True iff the current execution is determined to be a sanitizer run.
  """
  return True if os.environ.get("NH_INTEGRATION_TEST_SANITIZER_RUN", 0) == "1" else False


def isRunningInCircleCi():
  """Determine if the current execution is running in circleci.

  Depends on the environment variable CI=true which circleci sets by default.

  Returns:
      bool: True iff the current execution is running in circleci.
  """
  return True if os.environ.get("CI", "false") == "true" else False


def run_binary_with_args(binary, args):
  """Execute a Nighthawk binary with the provided arguments.

  Args:
    binary: A string, the name of the to-be-called binary, e.g. "nighthawk_client".
    args: A string, the command line arguments to the binary, e.g. "--foo --bar".

  Returns:
    A tuple in the form (exit_code, output), where exit_code is the code the Nighthawk
    service terminated with and the output is its standard output.
  """
  test_rundir = os.path.join(os.environ["TEST_SRCDIR"], os.environ["TEST_WORKSPACE"])
  args = "%s %s" % (os.path.join(test_rundir, binary), args)
  return subprocess.getstatusoutput(args)


def get_execution_duration_from_global_result_json(global_result_json):
  """Retrieve the actual execution duration from the global result.

  Args:
    global_result_json: A string, JSON representation of the Nighthawk's global
      result.

  Returns:
    A float, the actual execution duration in seconds.

  Raises:
    Error: if the global result doesn't contain the execution duration.
    Error: if the execution duration is in an unexpected format.
  """
  if "execution_duration" not in global_result_json:
    raise Error(
        "execution_duration not present in the global result:\n{}".format(global_result_json))

  # Encoded as a string that ends in the "s" suffix.
  # E.g. "3.000000001s".
  # https://googleapis.dev/ruby/google-cloud-scheduler/latest/Google/Protobuf/Duration.html
  duration_json_string = global_result_json["execution_duration"]

  if not duration_json_string.endswith('s'):
    raise Error(
        "the execution_duration '{} doesn't end with the expected suffix 's' for seconds".format(
            duration_json_string))
  return float(duration_json_string.rstrip('s'))
