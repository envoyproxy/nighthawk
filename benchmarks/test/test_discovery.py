#!/usr/bin/env python3
"""@package integration_test.py

Just a demo for now. Show how to tap into Nighthawk's
integration test framework to run benchmark executions.
"""

import logging
import json
import pytest
import os
from test.integration.integration_test_fixtures import (http_test_server_fixture,
                                                        https_test_server_fixture)
from test.integration.utility import *
from envoy_proxy import (inject_envoy_http_proxy_fixture, proxy_config)


def run_with_cpu_profiler(fixture,
                          rps=1000,
                          duration=1,
                          max_connections=1,
                          max_active_requests=1,
                          request_body_size=0,
                          response_size=1024,
                          concurrency=1):
  if hasattr(fixture, "proxy_server"):
    assert (fixture.proxy_server.enableCpuProfiler())
  assert (fixture.test_server.enableCpuProfiler())
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

  parsed_json, _ = fixture.runNighthawkClient(args)
  counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
  response_count = counters["benchmark.http_2xx"]
  request_count = counters["upstream_rq_total"]
  connection_counter = "upstream_cx_http1_total"

  # Some arbitrary sanity checks
  assertCounterGreaterEqual(counters, "benchmark.http_2xx", rps * duration)
  assertGreater(counters["upstream_cx_rx_bytes_total"], response_count * response_size)
  assertGreater(counters["upstream_cx_tx_bytes_total"], request_count * request_body_size)
  assertCounterEqual(counters, connection_counter, max_connections)

  # Could potentially set thresholds on acceptable latency here.

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


# Test via injected Envoy
@pytest.mark.parametrize('proxy_config', ["nighthawk/benchmarks/configurations/envoy_proxy.yaml"])
@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_http_h1_small_request_small_reply_via(inject_envoy_http_proxy_fixture, proxy_config):
  run_with_cpu_profiler(inject_envoy_http_proxy_fixture)


# Test the origin directly, using a stock fixture
@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def DISABLED_test_http_h1_small_request_small_reply_direct(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture)


@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_https_origin.yaml"])
def DISABLED_test_https_h1_small_request_small_reply_direct_s(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture)
