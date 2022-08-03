#!/usr/bin/env python3
"""Entry point for our integration testing."""
import logging
import os
import sys
import pytest

from test.integration import utility

if __name__ == '__main__':
  path = os.path.dirname(os.path.realpath(__file__))
  test_selection_arg = sys.argv[1] if len(sys.argv) > 1 else ""
  r = pytest.main(
      [
          "--rootdir=" + path,
          "-vvvv",
          "--showlocals",  # Don't abbreviate/truncate long values in asserts.
          "-p",
          "no:cacheprovider",  # Avoid a bunch of warnings on readonly filesystems
          "-k",
          test_selection_arg,  # Passed in via BUILD/py_test()
          "-x",
          path,
          "-n",
          "4" if utility.isSanitizerRun() else "20",  # Number of tests to run in parallel
          "--log-level",
          "INFO",
          "--log-cli-level",
          "INFO",
      ],
      plugins=["xdist"])
  exit(r)
