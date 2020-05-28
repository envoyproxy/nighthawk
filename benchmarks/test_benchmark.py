#!/usr/bin/env python3
"""@package integration_test.py

Just a demo for now. Show how to tap into Nighthawk's
integration test framework to run benchmark executions.
"""


import logging
import json
import pytest
from test.integration.integration_test_fixtures import http_test_server_fixture
from test.integration.utility import *
from infra import *

def run_with_cpu_profiler(fixture,
                          rps=10000,
                          duration=30,
                          max_connections=100,
                          max_active_requests=100,
                          request_body_size=0,
                          response_size=10,
                          concurrency=1):
  if hasattr(fixture, "proxy_server"):
    assert (fixture.proxy_server.enableCpuProfiler())
  assert (fixture.test_server.enableCpuProfiler())
  args = [
      fixture.getTestServerRootUri(),
      "--rps", str(rps),
      "--duration", str(duration),
      "--connections", str(max_connections),
      "--max-active-requests", str(max_active_requests),
      "--concurrency", str(concurrency),
      "--request-header", "x-nighthawk-test-server-config:{response_body_size:%s}" % response_size,
      "--experimental-h1-connection-reuse-strategy", "lru",
      "--prefetch-connections"
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
  assertCounterGreater(counters, "benchmark.http_2xx", 1000)
  assertGreater(counters["upstream_cx_rx_bytes_total"], response_count * response_size)
  assertGreater(counters["upstream_cx_tx_bytes_total"], request_count * request_body_size)
  assertCounterEqual(counters, connection_counter, max_connections)

  # Could potentially set thresholds on acceptable latency here.

  # dump human readably output to logs
  logging.info(fixture.transformNighthawkJson(json.dumps(parsed_json), "human"))

  # TODO(oschaaf): dump fortio/json/yaml/human output formats as artifacts

# Test via injected Envoy
def test_http_h1_small_request_small_reply_via(inject_envoy_http_proxy_fixture):
  run_with_cpu_profiler(inject_envoy_http_proxy_fixture)

# Test the origin directly, using a stock fixture 
def DISABLED_test_http_h1_small_request_small_reply_direct(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture)

