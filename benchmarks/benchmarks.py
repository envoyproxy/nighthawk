#!/usr/bin/env python3
"""@package benchmarks.

Entry point for benchmark execution.
"""
import os
import sys
import pytest

if __name__ == '__main__':
  path = os.path.dirname(os.path.realpath(__file__))
  r = pytest.main([
      "--rootdir=" + path, "-x", path, "-p", "no:cacheprovider", "--log-level", "INFO",
      "--log-cli-level", "INFO", *sys.argv
  ])
  exit(r)
