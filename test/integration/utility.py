"""Packages utility methods for tests."""

import os
import subprocess
import string
from typing import Union
from collections import namedtuple


class Error(Exception):
  """Raised on errors in this module."""


# Helper class to parse ip, port.
SocketAddress = namedtuple('SocketAddress', ['ip', 'port'])


def parseUrisToSocketAddress(uris: list[str]) -> list[SocketAddress]:
  """Parse a list of uris returning the corresponding list of SocketAddresses.

  Args:
    uris: List of uri strings of the format http://<ip>:<port>, the ip address can be IPv4 or IPv6.

  Returns:
    The corresponding list of SocketAddress for each URI.
  """
  addresses = []
  for uri in uris:
    ip_and_port = uri.split('/')[2]
    ip, port = ip_and_port.rsplit(':', maxsplit=1)
    addresses.append(SocketAddress(ip, int(port)))
  return addresses


def isSanitizerRun():
  """Determine if the current execution is a tsan/asan/ubsan run.

  Returns:
      bool: True iff the current execution is determined to be a sanitizer run.
  """
  return True if os.environ.get("NH_INTEGRATION_TEST_SANITIZER_RUN", 0) == "1" else False


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


def count_log_lines_with_substring(logs, substring):
  """Count the number of log lines containing the supplied substring.

  Args:
    logs: A string, Nighthawk client log output.
    substring: A string, the substring to search for.

  Returns:
    An integer, the number of log entries that contain the substring.
  """
  return len([line for line in logs.split(os.linesep) if substring in line])


def isRunningInAzpCi():
  """Determine if the current execution is running in the AZP CI.

  Depends on the environment variable AZP_BRANCH which is set in
  .azure-pipelines/bazel.yml.

  Returns:
      bool: True iff the current execution is running in the AZP CI.
  """
  return True if os.environ.get("AZP_BRANCH", "") else False


def substitute_yaml_values(runfiles_instance, obj: Union[dict, list, str], params: dict) -> str:
  """Substitute params into the given template.

  Args:
    runfiles_instance: A Runfiles instance.
    obj: Either a list of templates strings, a dict of template string or a template string.
    params: dict used to populate the provided templates.

  Returns:
      str: The template with the substituted parameters.
  """
  if isinstance(obj, dict):
    for k, v in obj.items():
      obj[k] = substitute_yaml_values(runfiles_instance, v, params)
  elif isinstance(obj, list):
    for i in range(len(obj)):
      obj[i] = substitute_yaml_values(runfiles_instance, obj[i], params)
  elif isinstance(obj, str):
    # Inspect string values and substitute where applicable.
    INJECT_RUNFILE_MARKER = '@inject-runfile:'
    if obj[0] == '$':
      return string.Template(obj).substitute(params)
    elif obj.startswith(INJECT_RUNFILE_MARKER):
      with open(runfiles_instance.Rlocation(obj[len(INJECT_RUNFILE_MARKER):].strip()), 'r') as file:
        return file.read()
  return obj
