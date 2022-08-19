#!/usr/bin/env python3
"""@package integration_test.

Test with Cluster churn with active traffic.
"""

import logging
import json
import pytest
import os

from dynamic_config_envoy_proxy import (dynamic_config_settings,
                                        inject_dynamic_envoy_http_proxy_fixture, proxy_config)
from rules_python.python.runfiles import runfiles
from shutil import copyfile
from test.integration import asserts


def _run_benchmark(fixture,
                   rps=1000,
                   duration=30,
                   max_connections=1,
                   max_active_requests=100,
                   request_body_size=0,
                   response_size=1024,
                   concurrency=1):
  args = [
      fixture.getTestServerRootUri(), "--rps",
      str(rps), "--duration",
      str(duration), "--connections",
      str(max_connections), "--max-active-requests",
      str(max_active_requests), "--concurrency",
      str(concurrency), "--request-header",
      "x-nighthawk-test-server-config:{response_body_size:%s}" % response_size,
      "--experimental-h1-connection-reuse-strategy", "lru", "--prefetch-connections"
  ]
  if request_body_size > 0:
    args.append("--request-body-size")
    args.append(str(request_body_size))

  parsed_json, _ = fixture.runNighthawkClient(args, expect_failure=True)

  # TODO(kbaichoo): refactor common test output to utilities.
  # dump human readable output to logs
  json_as_string = json.dumps(parsed_json)
  human_output = fixture.transformNighthawkJson(json_as_string, "human")
  logging.info(human_output)

  with open(os.path.join(fixture.test_server.tmpdir, "nighthawk-human.txt"), "w") as f:
    f.write(human_output)
  with open(os.path.join(fixture.test_server.tmpdir, "nighthawk.json"), "w") as f:
    f.write(json_as_string)
  with open(os.path.join(fixture.test_server.tmpdir, "nighthawk.yaml"), "w") as f:
    f.write(fixture.transformNighthawkJson(json_as_string, "yaml"))
  with open(os.path.join(fixture.test_server.tmpdir, "fortio.json"), "w") as f:
    f.write(fixture.transformNighthawkJson(json_as_string, "fortio"))
  with open(os.path.join(fixture.test_server.tmpdir, "server_version.txt"), "w") as f:
    f.write(fixture.test_server.getCliVersionString())
  if hasattr(fixture, "proxy_server"):
    with open(os.path.join(fixture.test_server.tmpdir, "proxy_version.txt"), "w") as f:
      f.write(fixture.proxy_server.getCliVersionString())
  r = runfiles.Create()
  copyfile(r.Rlocation("nighthawk/benchmarks/test/templates/simple_plot.html"),
           os.path.join(fixture.test_server.tmpdir, "simple_plot.html"))


# Test via injected Envoy
@pytest.mark.parametrize('proxy_config',
                         ["nighthawk/benchmarks/configurations/dynamic_resources.yaml"])
@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_dynamic_http(inject_dynamic_envoy_http_proxy_fixture, proxy_config):  # noqa
  _run_benchmark(inject_dynamic_envoy_http_proxy_fixture)
