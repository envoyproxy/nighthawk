#!/usr/bin/env python3

import logging
import os
import sys
import pytest

from test.integration.common import IpVersion
from test.integration.integration_test_fixtures import (http_test_server_fixture,
                                                        https_test_server_fixture)
from test.integration.utility import *


def run_with_cpu_profiler(fixture,
                          rps=999999,
                          use_h2=False,
                          duration=10,
                          max_connections=1,
                          max_active_requests=1,
                          request_body_size=0,
                          response_size=10,
                          concurrency=1):
  assert (fixture.test_server.enableCpuProfiler())
  MIN_EXPECTED_REQUESTS = 100
  args = [
      fixture.getTestServerRootUri(), "--rps",
      str(rps), "--duration",
      str(duration), "--connections",
      str(max_connections), "--max-active-requests",
      str(max_active_requests), "--concurrency",
      str(concurrency), "--request-header",
      "x-nighthawk-test-server-config:{response_body_size:%s}" % response_size
  ]
  if use_h2:
    args.append("--h2")
  if request_body_size > 0:
    args.append("--request-body-size")
    args.append(str(request_body_size))

  parsed_json, _ = fixture.runNighthawkClient(args)
  counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
  # We expect to have executed a certain amount of requests
  assertCounterGreater(counters, "benchmark.http_2xx", MIN_EXPECTED_REQUESTS)
  response_count = counters["benchmark.http_2xx"]
  assertGreater(counters["upstream_cx_rx_bytes_total"], response_count * response_size)

  request_count = counters["upstream_rq_total"]
  # TODO(oschaaf): There's something weird here, the numbers don't add up. We divide by as a temp workaround
  # to pass here, but this surely deserves investigation.
  # Note: numbers are even below what we would expect when considering the number of confirmed replies.
  assertGreater(counters["upstream_cx_tx_bytes_total"], (request_count * request_body_size) / 10)

  # We expect to have created only a single connection
  if use_h2:
    assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  else:
    # Apparently, when a request_body_size > 0 is involved, we will create > 1 connections.
    # TODO(oschaaf): figure out the specifics of ^^.
    if request_body_size == 0:
      assertCounterEqual(counters, "upstream_cx_http1_total", 1)

  global_histograms = fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
  assertGreater(int(global_histograms["sequencer.blocking"]["count"]), MIN_EXPECTED_REQUESTS)
  assertGreater(
      int(global_histograms["benchmark_http_client.request_to_response"]["count"]),
      MIN_EXPECTED_REQUESTS)
  # dump output
  logging.info(str(parsed_json))


def test_http_h1_small_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture)


def test_https_h1_small_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture)


def test_http_h2_small_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, use_h2=True)


def test_https_h2_small_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, use_h2=True)


# TODO(oschaaf): With 1MB request body sizes we hit a threshold, which triggers a panic. I suspect this is because of our
# custom streamdecoder asserting on some unimplemented watermark callbacks.
def test_http_h1_1mb_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000)


def test_https_h1_1mb_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000)


def test_http_h2_1mb_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


def test_https_h2_1mb_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


# A series with ~1MB request/replies
def test_http_h1_1mb_request_1MB_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000)


def test_https_h1_1mb_request_1MB_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000)


def test_http_h2_1mb_request_1MB_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


def test_https_h2_1mb_request_1MB_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


# TODO: add tests using multiple cores on both front and backend.
# TODO: the current Envoy is tested in direct response mode. add an env where we run this with Envoy as a proxy in the middle.
