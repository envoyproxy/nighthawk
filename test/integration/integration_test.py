#! /home/venv/bin/python
"""@package integration_test.py
Entry point for our integration testing
"""
import logging
import os
import sys
import pytest

if __name__ == '__main__':
  path = os.path.dirname(os.path.realpath(__file__))
  r = pytest.main(["--rootdir=" + path, "-x", path, "-n", "20"], plugins=["xdist"])
  exit(r)
