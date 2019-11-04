#!/usr/bin/env python3

import logging
import os
import sys
import pytest

from test.integration.common import IpVersion
from test.integration.integration_test_fixtures import (http_test_server_fixture,
                                                        https_test_server_fixture)
from test.integration.utility import *

# TODO(oschaaf): we mostly verify stats observed from the client-side. Add expectations
# for the server side as well.


def test_http_h1(http_test_server_fixture):
  """
  Runs the CLI configured to use plain HTTP/1 against our test server, and sanity
  checks statistics from both client and server.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient(
      [http_test_server_fixture.getTestServerRootUri()])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http1_total", 1)
  assertCounterEqual(counters, "upstream_cx_rx_bytes_total", 3400)
  assertCounterEqual(counters, "upstream_cx_total", 1)
  assertCounterEqual(counters, "upstream_cx_tx_bytes_total",
                     1400 if http_test_server_fixture.ip_version == IpVersion.IPV6 else 1500)
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertCounterEqual(counters, "upstream_rq_total", 25)
  assertCounterEqual(counters, "default.total_match_count", 1)
  assertEqual(len(counters), 12)


def mini_stress_test(fixture, args):
  # run a test with more rps then we can handle, and a very small client-side queue.
  # we should observe both lots of successfull requests as well as time spend in blocking mode.,
  parsed_json, _ = fixture.runNighthawkClient(args)
  counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
  # We set a reasonably low expectation of 100 requests. We set it low, because we want this
  # test to succeed on a reasonable share of setups (hopefully practically all).
  MIN_EXPECTED_REQUESTS = 100
  assertCounterGreater(counters, "benchmark.http_2xx", MIN_EXPECTED_REQUESTS)
  if "--h2" in args:
    assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  else:
    assertCounterEqual(counters, "upstream_cx_http1_total", 1)
  global_histograms = fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
  assertGreater(int(global_histograms["sequencer.blocking"]["count"]), MIN_EXPECTED_REQUESTS)
  assertGreater(
      int(global_histograms["benchmark_http_client.request_to_response"]["count"]),
      MIN_EXPECTED_REQUESTS)
  return counters


# The mini stress tests below are executing in closed-loop mode. As we guard the pool against
# overflows, we can set fixed expectations with respect to overflows and anticipated pending
# totals.
def test_http_h1_mini_stress_test_with_client_side_queueing(http_test_server_fixture):
  """
  Run a max rps test with the h1 pool against our test server, using a small client-side
  queue."""
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "999999", "--max-pending-requests",
      "10", "--duration 10", "--connections", "1"
  ])
  assertCounterEqual(counters, "upstream_rq_pending_total", 11)
  assertCounterEqual(counters, "upstream_cx_overflow", 10)


def test_http_h1_mini_stress_test_without_client_side_queueing(http_test_server_fixture):
  """
  Run a max rps test with the h1 pool against our test server, with no client-side
  queueing.
  """
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "999999", "--duration 2",
      "--connections", "1"
  ])
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertNotIn("upstream_cx_overflow", counters)


def test_http_h2_mini_stress_test_with_client_side_queueing(http_test_server_fixture):
  """
  Run a max rps test with the h2 pool against our test server, using a small client-side
  queue. 
  """
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "999999", "--max-pending-requests",
      "10", "--duration 10", "--h2", "--max-active-requests", "1", "--connections", "1"
  ])
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertCounterEqual(counters, "upstream_rq_pending_overflow", 10)


def test_http_h2_mini_stress_test_without_client_side_queueing(http_test_server_fixture):
  """
  Run a max rps test with the h2 pool against our test server, with no client-side
  queueing. 
  """
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "999999", "--duration 2", "--h2",
      "--max-active-requests", "1", "--connections", "1"
  ])
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertNotIn("upstream_rq_pending_overflow", counters)


def test_http_h2(http_test_server_fixture):
  """
  Runs the CLI configured to use h2c against our test server, and sanity
  checks statistics from both client and server.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient(
      ["--h2", http_test_server_fixture.getTestServerRootUri()])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_rx_bytes_total", 1145)
  assertCounterEqual(counters, "upstream_cx_total", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_tx_bytes_total", 403)
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertCounterEqual(counters, "upstream_rq_total", 25)
  assertCounterEqual(counters, "default.total_match_count", 1)
  assertEqual(len(counters), 12)


def test_http_concurrency(http_test_server_fixture):
  """
  Concurrency should act like a multiplier.
  """

  parsed_json, _ = http_test_server_fixture.runNighthawkClient(
      ["--concurrency 4 --rps 5 --connections 1",
       http_test_server_fixture.getTestServerRootUri()])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)

  # Quite a loose expectation, but this may fluctuate depending on server load.
  # Ideally we'd see 4 workers * 5 rps * 5s = 100 requests total
  assertCounterGreater(counters, "benchmark.http_2xx", 25)
  assertCounterLessEqual(counters, "benchmark.http_2xx", 100)
  assertCounterEqual(counters, "upstream_cx_http1_total", 4)


def test_https_h1(https_test_server_fixture):
  """
  Runs the CLI configured to use HTTP/1 over https against our test server, and sanity
  checks statistics from both client and server.
  """
  parsed_json, _ = https_test_server_fixture.runNighthawkClient(
      [https_test_server_fixture.getTestServerRootUri()])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http1_total", 1)
  assertCounterEqual(counters, "upstream_cx_rx_bytes_total", 3400)
  assertCounterEqual(counters, "upstream_cx_total", 1)
  assertCounterEqual(counters, "upstream_cx_tx_bytes_total",
                     1400 if https_test_server_fixture.ip_version == IpVersion.IPV6 else 1500)
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertCounterEqual(counters, "upstream_rq_total", 25)
  assertCounterEqual(counters, "ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256", 1)
  assertCounterEqual(counters, "ssl.curves.X25519", 1)
  assertCounterEqual(counters, "ssl.handshake", 1)
  assertCounterEqual(counters, "ssl.sigalgs.rsa_pss_rsae_sha256", 1)
  assertCounterEqual(counters, "ssl.versions.TLSv1.2", 1)
  assertCounterEqual(counters, "default.total_match_count", 1)
  assertEqual(len(counters), 17)

  server_stats = https_test_server_fixture.getTestServerStatisticsJson()
  assertEqual(
      https_test_server_fixture.getServerStatFromJson(server_stats,
                                                      "http.ingress_http.downstream_rq_2xx"), 25)


def test_https_h2(https_test_server_fixture):
  """
  Runs the CLI configured to use HTTP/2 (using https) against our test server, and sanity
  checks statistics from both client and server.
  """

  parsed_json, _ = https_test_server_fixture.runNighthawkClient(
      ["--h2", https_test_server_fixture.getTestServerRootUri()])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_rx_bytes_total", 1145)
  assertCounterEqual(counters, "upstream_cx_total", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_tx_bytes_total", 403)
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertCounterEqual(counters, "upstream_rq_total", 25)
  assertCounterEqual(counters, "ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256", 1)
  assertCounterEqual(counters, "ssl.curves.X25519", 1)
  assertCounterEqual(counters, "ssl.handshake", 1)
  assertCounterEqual(counters, "ssl.sigalgs.rsa_pss_rsae_sha256", 1)
  assertCounterEqual(counters, "ssl.versions.TLSv1.2", 1)
  assertCounterEqual(counters, "default.total_match_count", 1)
  assertEqual(len(counters), 17)


def test_https_h1_tls_context_configuration(https_test_server_fixture):
  """
  Verifies specifying tls cipher suites works with the h1 pool
  """

  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      "--duration 1",
      "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES128-SHA\"]}}}",
      https_test_server_fixture.getTestServerRootUri()
  ])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "ssl.ciphers.ECDHE-RSA-AES128-SHA", 1)

  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      "--h2", "--duration 1",
      "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-CHACHA20-POLY1305\"]}}}",
      https_test_server_fixture.getTestServerRootUri()
  ])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "ssl.ciphers.ECDHE-RSA-CHACHA20-POLY1305", 1)


def test_https_h2_tls_context_configuration(https_test_server_fixture):
  """
  Verifies specifying tls cipher suites works with the h2 pool
  """
  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      "--duration 1",
      "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES128-SHA\"]}}}",
      https_test_server_fixture.getTestServerRootUri()
  ])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "ssl.ciphers.ECDHE-RSA-AES128-SHA", 1)

  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      "--h2", "--duration 1",
      "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-CHACHA20-POLY1305\"]}}}",
      https_test_server_fixture.getTestServerRootUri()
  ])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "ssl.ciphers.ECDHE-RSA-CHACHA20-POLY1305", 1)


def test_https_prefetching(https_test_server_fixture):
  """
  Test we prefetch connections. We test for 1 second at 1 rps, which should
  result in 1 connection max without prefetching. However, we specify 50 connections
  and the prefetching flag, so we ought to see 50 http1 connections created.
  """
  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      "--duration 1", "--rps 1", "--prefetch-connections", "--connections 50",
      https_test_server_fixture.getTestServerRootUri()
  ])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "upstream_cx_http1_total", 50)


def test_https_log_verbosity(https_test_server_fixture):
  """
  Test that that the specified log verbosity level is respected.
  This tests for a sentinel we know is only right when the level
  is set to 'trace'.
  """
  # TODO(oschaaf): this is kind of fragile. Can we improve?
  trace_level_sentinel = "nighthawk_service_zone"
  _, logs = https_test_server_fixture.runNighthawkClient(
      ["--duration 1", "--rps 1", "-v debug",
       https_test_server_fixture.getTestServerRootUri()])
  assertNotIn(trace_level_sentinel, logs)

  _, logs = https_test_server_fixture.runNighthawkClient(
      ["--duration 1", "--rps 1", "-v trace",
       https_test_server_fixture.getTestServerRootUri()])
  assertIn(trace_level_sentinel, logs)


def test_dotted_output_format(http_test_server_fixture):
  """
  Ensure we get the dotted string output format when requested.
  and ensure we get latency percentiles.
  """
  output, _ = http_test_server_fixture.runNighthawkClient([
      "--duration 1", "--rps 10", "--output-format dotted",
      http_test_server_fixture.getTestServerRootUri()
  ],
                                                          as_json=False)
  assertIn("global.benchmark_http_client.request_to_response.permilles-500.microseconds", output)


# TODO(oschaaf): add percentiles to the gold testing in the C++ output formatter
# once the fortio formatter has landed (https://github.com/envoyproxy/nighthawk/pull/168)
def test_cli_output_format(http_test_server_fixture):
  """
  Ensure we observe latency percentiles with CLI output.
  """
  output, _ = http_test_server_fixture.runNighthawkClient(
      ["--duration 1", "--rps 10",
       http_test_server_fixture.getTestServerRootUri()], as_json=False)
  assertIn("Initiation to completion", output)
  assertIn("Percentile", output)


def test_request_body_gets_transmitted(http_test_server_fixture):
  """
  Test that the number of bytes we request for the request body gets reflected in the upstream
  connection transmitted bytes counter for h1 and h2.
  """

  def check_upload_expectations(fixture, parsed_json, expected_transmitted_bytes,
                                expected_received_bytes):
    counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
    assertCounterGreaterEqual(counters, "upstream_cx_tx_bytes_total", expected_transmitted_bytes)
    server_stats = fixture.getTestServerStatisticsJson()
    assertGreaterEqual(
        fixture.getServerStatFromJson(server_stats,
                                      "http.ingress_http.downstream_cx_rx_bytes_total"),
        expected_received_bytes)

  upload_bytes = 10000

  # test h1
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "1", "--rps", "2",
      "--request-body-size",
      str(upload_bytes)
  ])

  # We expect rps * upload_bytes to be transferred/received.
  check_upload_expectations(http_test_server_fixture, parsed_json, upload_bytes * 2,
                            upload_bytes * 2)

  # test h2
  # Again, we expect rps * upload_bytes to be transferred/received. However, we didn't reset
  # the server in between, so our expectation for received bytes on the server side is raised.
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "1", "--h2", "--rps", "2",
      "--request-body-size",
      str(upload_bytes)
  ])
  check_upload_expectations(http_test_server_fixture, parsed_json, upload_bytes * 2,
                            upload_bytes * 4)
