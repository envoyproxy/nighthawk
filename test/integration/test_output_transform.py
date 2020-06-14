#!/usr/bin/env python3
import pytest

from test.integration.utility import *
import os
import subprocess


def run_output_transform_with_args(args):
  return run_binary_with_args("nighthawk_output_transform", args)


def test_output_transform_help():
  (exit_code, output) = run_output_transform_with_args("--help")
  assert (exit_code == 0)
  assert ("USAGE" in output)


def test_output_transform_bad_arguments():
  (exit_code, output) = run_output_transform_with_args("--foo")
  assert (exit_code == 1)
  assert ("PARSE ERROR: Argument: --foo" in output)


def test_output_transform_101():
  """
  Runs an arbitrary load test, which outputs to json.
  This json output is then transformed to human readable output.
  """

  test_rundir = os.path.join(os.environ["TEST_SRCDIR"], os.environ["TEST_WORKSPACE"])
  process = subprocess.run([
      os.path.join(test_rundir, "nighthawk_client"), "--duration", "1", "--rps", "1", "127.0.0.1",
      "--output-format", "json"
  ],
                           stdout=subprocess.PIPE)
  output = process.stdout
  process = subprocess.run(
      [os.path.join(test_rundir, "nighthawk_output_transform"), "--output-format", "human"],
      stdout=subprocess.PIPE,
      input=output)
  assert (process.returncode == 0)
  assert ("Nighthawk - A layer 7 protocol benchmarking tool" in process.stdout.decode("utf-8"))
