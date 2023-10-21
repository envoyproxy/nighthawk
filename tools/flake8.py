"""Wrapper around flake8.

Exists, so that we can define a Bazel py_binary target that combines flake8 and
any flake8 plugins we might need.

"""
import flake8.main.cli

import sys

if __name__ == '__main__':
  sys.exit(flake8.main.cli.main())
