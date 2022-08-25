"""Utilities for benchmark tests."""

import logging
import json
from rules_python.python.runfiles import runfiles
import os
from shutil import copyfile


def output_benchmark_results(json_results, fixture):
  """Output the benchmark results."""
  output_dir = fixture.test_server.tmpdir
  json_as_string = json.dumps(json_results)
  human_output = fixture.transformNighthawkJson(json_as_string, "human")
  logging.info(human_output)

  with open(os.path.join(output_dir, "nighthawk-human.txt"), "w") as f:
    f.write(human_output)
  with open(os.path.join(output_dir, "nighthawk.json"), "w") as f:
    f.write(json_as_string)
  with open(os.path.join(output_dir, "nighthawk.yaml"), "w") as f:
    f.write(fixture.transformNighthawkJson(json_as_string, "yaml"))
  with open(os.path.join(output_dir, "fortio.json"), "w") as f:
    f.write(fixture.transformNighthawkJson(json_as_string, "fortio"))
  with open(os.path.join(output_dir, "server_version.txt"), "w") as f:
    f.write(fixture.test_server.getCliVersionString())
  if hasattr(fixture, "proxy_server"):
    with open(os.path.join(output_dir, "proxy_version.txt"), "w") as f:
      f.write(fixture.proxy_server.getCliVersionString())
  r = runfiles.Create()
  copyfile(r.Rlocation("nighthawk/benchmarks/templates/simple_plot.html"),
           os.path.join(fixture.test_server.tmpdir, "simple_plot.html"))
