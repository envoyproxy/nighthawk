import os
import subprocess


def isSanitizerRun():
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
