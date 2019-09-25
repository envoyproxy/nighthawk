#!/usr/bin/env python3
"""@package integration_test.py
Entry point for our integration testing
"""
import logging
import os
import sys
import pytest

if __name__ == '__main__':
  path = os.path.dirname(os.path.realpath(__file__))
  test_selection_arg = sys.argv[1] if len(sys.argv) > 1 else ""
  r = pytest.main(["--rootdir=" + path, "-k", test_selection_arg, "-x", path])
  exit(r)
