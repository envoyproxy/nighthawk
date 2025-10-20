#!/usr/bin/env python3
"""Entry point for our integration testing."""
import logging
import os
import sys
import multiprocessing
import pytest

from test.integration import utility

if __name__ == '__main__':
  path = os.path.dirname(os.path.realpath(__file__))
  test_selection_arg = sys.argv[1] if len(sys.argv) > 1 else ""
  num_cores = multiprocessing.cpu_count()
  num_workers = max(1, min(num_cores - 1, 4 if utility.isSanitizerRun() else num_cores))

  r = pytest.main(
      [
          "--rootdir=" + path,
          "-p",
          "no:cacheprovider",  # Avoid a bunch of warnings on readonly filesystems
          "-k",
          test_selection_arg,  # Passed in via BUILD/py_test()
          "-m"
          "serial",
          "-x",
          path,
          "-n",
          "1",  # Run in serial
          "--log-level",
          "INFO",
          "--log-cli-level",
          "INFO",
      ],
      plugins=["xdist"])
#   if (r != 0):
#     exit(r)
#   r = pytest.main(
#       [
#           "--rootdir=" + path,
#           "-p",
#           "no:cacheprovider",  # Avoid a bunch of warnings on readonly filesystems
#           "-k",
#           test_selection_arg,  # Passed in via BUILD/py_test()
#           "-m"
#           "not serial",
#           "-x",
#           path,
#           "-n",
#           str(num_workers),
#           "--log-level",
#           "INFO",
#           "--log-cli-level",
#           "INFO",
#       ],
#       plugins=["xdist"])
  exit(r)
