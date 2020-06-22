#!/usr/bin/env python3
import pytest

from test.integration import utility
import os
import subprocess


def _run_output_transform_with_args(args):
  return utility.run_binary_with_args("nighthawk_output_transform", args)


def test_output_transform_help():
  (exit_code, output) = _run_output_transform_with_args("--help")
  utility.assertEqual(exit_code, 0)
  utility.assertIn("USAGE", output)


def test_output_transform_bad_arguments():
  (exit_code, output) = _run_output_transform_with_args("--foo")
  utility.assertEqual(exit_code, 1)
  utility.assertIn("PARSE ERROR: Argument: --foo", output)


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
  utility.assertEqual(process.returncode, 0)
  utility.assertIn("Nighthawk - A layer 7 protocol benchmarking tool",
                   process.stdout.decode("utf-8"))
