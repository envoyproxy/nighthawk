#!/usr/bin/env python3
"""@package benchmarks.

Entry point for benchmark execution.
"""
import os
import pytest
import sys

# Workaround for https://github.com/bazelbuild/rules_python/issues/1221
sys.path += [os.path.dirname(__file__)]

if __name__ == '__main__':
  path = os.path.dirname(__file__)
  r = pytest.main([
      "--rootdir=" + path, "-x", (path + '/dynamic_test/'), "-p", "no:cacheprovider", "--log-level",
      "INFO", "--log-cli-level", "INFO", *sys.argv
  ])
  exit(r)
