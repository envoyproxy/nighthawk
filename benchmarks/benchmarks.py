#!/usr/bin/env python3
"""@package integration_test.py
Entry point benchmark tests
"""
import os
import sys
import pytest

if __name__ == '__main__':
  path = os.path.dirname(os.path.realpath(__file__))
  r = pytest.main(["--rootdir=" + path, "-x", path, *sys.argv])
  exit(r)
