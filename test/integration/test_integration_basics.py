#!/usr/bin/env python3

import logging
import os
import sys
import pytest

from test.integration.common import IpVersion
from test.integration.integration_test_fixtures import (
    http_test_server_fixture, https_test_server_fixture, multi_http_test_server_fixture,
    multi_https_test_server_fixture, sni_test_server_fixture)
from test.integration.utility import *

# TODO(oschaaf): we mostly verify stats observed from the client-side. Add expectations
# for the server side as well.


def test_http_h1(http_test_server_fixture):
  """
  Runs the CLI configured to use plain HTTP/1 against our test server, and sanity
  checks statistics from both client and server.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:24"
  ])
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

  global_histograms = http_test_server_fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
  assertEqual(int(global_histograms["benchmark_http_client.response_body_size"]["count"]), 25)
  assertEqual(int(global_histograms["benchmark_http_client.response_header_size"]["count"]), 25)
  assertEqual(int(global_histograms["benchmark_http_client.response_body_size"]["raw_mean"]), 10)
  assertEqual(int(global_histograms["benchmark_http_client.response_header_size"]["raw_mean"]), 97)
  assertEqual(int(global_histograms["benchmark_http_client.response_body_size"]["raw_min"]), 10)
  assertEqual(int(global_histograms["benchmark_http_client.response_header_size"]["raw_min"]), 97)
  assertEqual(int(global_histograms["benchmark_http_client.response_body_size"]["raw_max"]), 10)
  assertEqual(int(global_histograms["benchmark_http_client.response_header_size"]["raw_max"]), 97)
  assertEqual(int(global_histograms["benchmark_http_client.response_body_size"]["raw_pstdev"]), 0)
  assertEqual(int(global_histograms["benchmark_http_client.response_header_size"]["raw_pstdev"]), 0)

  assertEqual(len(counters), 12)


def mini_stress_test(fixture, args):
  # run a test with more rps then we can handle, and a very small client-side queue.
  # we should observe both lots of successfull requests as well as time spend in blocking mode.,
  parsed_json, _ = fixture.runNighthawkClient(args)
  counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
  # We set a reasonably low expectation of 100 requests. We set it low, because we want this
  # test to succeed on a reasonable share of setups (hopefully practically all).
  MIN_EXPECTED_REQUESTS = 100
  assertCounterEqual(counters, "benchmark.http_2xx", MIN_EXPECTED_REQUESTS)
  if "--h2" in args:
    assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  else:
    assertCounterEqual(counters, "upstream_cx_http1_total", 1)
  global_histograms = fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)

  if "--open-loop" in args:
    assertEqual(int(global_histograms["sequencer.blocking"]["count"]), 0)
  else:
    assertGreaterEqual(int(global_histograms["sequencer.blocking"]["count"]), 1)

  assertGreaterEqual(
      int(global_histograms["benchmark_http_client.request_to_response"]["count"]), 1)
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
      "10", "--connections", "1", "--duration", "100", "--termination-predicate",
      "benchmark.http_2xx:99"
  ])
  assertCounterEqual(counters, "upstream_rq_pending_total", 11)
  assertCounterEqual(counters, "upstream_cx_overflow", 10)


def test_http_h1_mini_stress_test_without_client_side_queueing(http_test_server_fixture):
  """
  Run a max rps test with the h1 pool against our test server, with no client-side
  queueing.
  """
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "999999", "--connections", "1",
      "--duration", "100", "--termination-predicate", "benchmark.http_2xx:99"
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
      "10", "--h2", "--max-active-requests", "1", "--connections", "1", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:99"
  ])
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertCounterEqual(counters, "upstream_rq_pending_overflow", 10)


def test_http_h2_mini_stress_test_without_client_side_queueing(http_test_server_fixture):
  """
  Run a max rps test with the h2 pool against our test server, with no client-side
  queueing. 
  """
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "999999", "--h2",
      "--max-active-requests", "1", "--connections", "1", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:99"
  ])
  assertCounterEqual(counters, "upstream_rq_pending_total", 1)
  assertNotIn("upstream_rq_pending_overflow", counters)


@pytest.mark.skipif(isSanitizerRun(), reason="Unstable and very slow in sanitizer runs")
def test_http_h1_mini_stress_test_open_loop(http_test_server_fixture):
  """
  H1 open loop stress test. We expect higher pending and overflow counts 
  """
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "10000", "--max-pending-requests",
      "1", "--open-loop", "--max-active-requests", "1", "--connections", "1", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:99"
  ])
  # we expect pool overflows
  assertCounterGreater(counters, "benchmark.pool_overflow", 10)


@pytest.mark.skipif(isSanitizerRun(), reason="Unstable and very slow in sanitizer runs")
def test_http_h2_mini_stress_test_open_loop(http_test_server_fixture):
  """
  H2 open loop stress test. We expect higher overflow counts 
  """
  counters = mini_stress_test(http_test_server_fixture, [
      http_test_server_fixture.getTestServerRootUri(), "--rps", "10000", "--max-pending-requests",
      "1", "--h2", "--open-loop", "--max-active-requests", "1", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:99"
  ])
  # we expect pool overflows
  assertCounterGreater(counters, "benchmark.pool_overflow", 10)


def test_http_h2(http_test_server_fixture):
  """
  Runs the CLI configured to use h2c against our test server, and sanity
  checks statistics from both client and server.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--h2",
      http_test_server_fixture.getTestServerRootUri(), "--max-active-requests", "1", "--duration",
      "100", "--termination-predicate", "benchmark.http_2xx:24", "--rps", "100"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_rx_bytes_total", 1030)
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

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--concurrency 4 --rps 100 --connections 1", "--duration", "100", "--termination-predicate",
      "benchmark.http_2xx:24",
      http_test_server_fixture.getTestServerRootUri()
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)

  # Quite a loose expectation, but this may fluctuate depending on server load.
  # Ideally we'd see 4 workers * 5 rps * 5s = 100 requests total
  assertCounterEqual(counters, "benchmark.http_2xx", 100)
  assertCounterEqual(counters, "upstream_cx_http1_total", 4)


def test_https_h1(https_test_server_fixture):
  """
  Runs the CLI configured to use HTTP/1 over https against our test server, and sanity
  checks statistics from both client and server.
  """
  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      https_test_server_fixture.getTestServerRootUri(), "--connections", "1", "--rps", "100",
      "--duration", "100", "--termination-predicate", "benchmark.http_2xx:24"
  ])
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
  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      "--h2",
      https_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:24", "--max-active-requests", "1"
  ])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  # Through emperical observation, 1030 has been determined to be the minimum of bytes
  # we can expect to have received when execution has stopped.
  assertCounterGreaterEqual(counters, "upstream_cx_rx_bytes_total", 1030)
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


def test_https_h2_multiple_connections(https_test_server_fixture):
  """
  Test that the experimental h2 pool uses multiple connections.
  """
  parsed_json, _ = https_test_server_fixture.runNighthawkClient([
      "--h2",
      https_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:9", "--max-active-requests", "1",
      "--experimental-h2-use-multiple-connections"
  ])
  counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 10)
  assertCounterEqual(counters, "upstream_cx_http2_total", 10)


def _do_tls_configuration_test(https_test_server_fixture, cli_parameter, use_h2):
  """Runs tests for different ciphers.

  For a given choice of (--tls-context, --transport-socket) x (H1, H2),
  run a series of traffic tests with different ciphers.

  Args:
    https_test_server_fixture: pytest.fixture that controls a test server and client
    cli_parameter: string, --tls-context or --transport-socket
    use_h2: boolean, whether to pass --h2
  """

  if cli_parameter == "--tls-context":
    json_template = "{common_tls_context:{tls_params:{cipher_suites:[\"-ALL:%s\"]}}}"
  else:
    json_template = ("{name:\"envoy.transport_sockets.tls\",typed_config:{" +
                     "\"@type\":\"type.googleapis.com/envoy.api.v2.auth.UpstreamTlsContext\"," +
                     "common_tls_context:{tls_params:{cipher_suites:[\"-ALL:%s\"]}}}}")

  for cipher in [
      "ECDHE-RSA-AES128-SHA",
      "ECDHE-RSA-CHACHA20-POLY1305",
  ]:
    parsed_json, _ = https_test_server_fixture.runNighthawkClient((["--h2"] if use_h2 else []) + [
        "--termination-predicate", "benchmark.http_2xx:0", cli_parameter, json_template % cipher,
        https_test_server_fixture.getTestServerRootUri()
    ])
    counters = https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
    assertCounterEqual(counters, "ssl.ciphers.%s" % cipher, 1)


def test_https_h1_tls_context_configuration(https_test_server_fixture):
  """
  Verifies specifying tls cipher suites works with the h1 pool
  """
  _do_tls_configuration_test(https_test_server_fixture, "--tls-context", use_h2=False)


def test_https_h1_transport_socket_configuration(https_test_server_fixture):
  """
  Verifies specifying tls cipher suites via transport socket works with the h1 pool
  """

  _do_tls_configuration_test(https_test_server_fixture, "--transport-socket", use_h2=False)


def test_https_h2_tls_context_configuration(https_test_server_fixture):
  """
  Verifies specifying tls cipher suites works with the h2 pool
  """
  _do_tls_configuration_test(https_test_server_fixture, "--tls-context", use_h2=True)


def test_https_h2_transport_socket_configuration(https_test_server_fixture):
  """
  Verifies specifying tls cipher suites via transport socket works with the h2 pool
  """
  _do_tls_configuration_test(https_test_server_fixture, "--transport-socket", use_h2=True)


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
    # Server side expectations start failing with larger upload sizes
    assertGreaterEqual(
        fixture.getServerStatFromJson(server_stats,
                                      "http.ingress_http.downstream_cx_rx_bytes_total"),
        expected_received_bytes)

  upload_bytes = 1024 * 1024 * 3
  requests = 10
  args = [
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100", "--rps", "100",
      "--request-body-size",
      str(upload_bytes), "--termination-predicate",
      "benchmark.http_2xx:%s" % str(requests), "--connections", "1", "--request-method", "POST",
      "--max-active-requests", "1"
  ]
  # Test we transmit the expected amount of bytes with H1
  parsed_json, _ = http_test_server_fixture.runNighthawkClient(args)
  check_upload_expectations(http_test_server_fixture, parsed_json, upload_bytes * requests,
                            upload_bytes * requests)

  # Test we transmit the expected amount of bytes with H2
  args.append("--h2")
  parsed_json, _ = http_test_server_fixture.runNighthawkClient(args)
  # We didn't reset the server in between, so our expectation for received bytes on the server side is raised.
  check_upload_expectations(http_test_server_fixture, parsed_json, upload_bytes * requests,
                            upload_bytes * requests * 2)


def test_http_h1_termination_predicate(http_test_server_fixture):
  """
  Put in a termination predicate. Should result in successfull execution, with 10 successfull requests.
  We would expect 25 based on rps and duration.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "5", "--rps", "500",
      "--connections", "1", "--termination-predicate", "benchmark.http_2xx:9"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 10)


def test_http_h1_failure_predicate(http_test_server_fixture):
  """
  Put in a termination predicate. Should result in failing execution, with 10 successfull requests.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "5", "--rps", "500",
      "--connections", "1", "--failure-predicate", "benchmark.http_2xx:0"
  ],
                                                               expect_failure=True)
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 1)


def test_bad_arg_error_messages(http_test_server_fixture):
  """
  Test arguments that pass proto validation, but are found to be no good nonetheless, result in reasonable error
  messages.
  """
  _, err = http_test_server_fixture.runNighthawkClient(
      [http_test_server_fixture.getTestServerRootUri(), "--termination-predicate ", "a:a"],
      expect_failure=True,
      as_json=False)
  assert "Bad argument: Termination predicate 'a:a' has an out of range threshold." in err


def test_multiple_backends_http_h1(multi_http_test_server_fixture):
  """
  Runs the CLI configured to use plain HTTP/1 against multiple test servers, and sanity
  checks statistics from both client and server.
  """
  nighthawk_client_args = [
      "--multi-target-path", "/", "--duration", "100", "--termination-predicate",
      "benchmark.http_2xx:24"
  ]
  for uri in multi_http_test_server_fixture.getAllTestServerRootUris():
    nighthawk_client_args.append("--multi-target-endpoint")
    nighthawk_client_args.append(uri.replace("http://", "").replace("/", ""))

  parsed_json, stderr = multi_http_test_server_fixture.runNighthawkClient(nighthawk_client_args)

  counters = multi_http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http1_total", 3)
  assertCounterGreater(counters, "upstream_cx_rx_bytes_total", 0)
  assertCounterEqual(counters, "upstream_cx_total", 3)
  assertCounterGreater(counters, "upstream_cx_tx_bytes_total", 0)
  assertCounterEqual(counters, "upstream_rq_pending_total", 3)
  assertCounterEqual(counters, "upstream_rq_total", 25)
  assertCounterEqual(counters, "default.total_match_count", 3)
  total_2xx = 0
  for parsed_server_json in multi_http_test_server_fixture.getAllTestServerStatisticsJsons():
    single_2xx = multi_http_test_server_fixture.getServerStatFromJson(
        parsed_server_json, "http.ingress_http.downstream_rq_2xx")
    assertBetweenInclusive(single_2xx, 8, 9)
    total_2xx += single_2xx
  assertBetweenInclusive(total_2xx, 24, 25)


def test_multiple_backends_https_h1(multi_https_test_server_fixture):
  """
  Runs the CLI configured to use HTTP/1 with TLS against multiple test servers, and sanity
  checks statistics from both client and server.
  """
  nighthawk_client_args = [
      "--multi-target-use-https", "--multi-target-path", "/", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:24"
  ]
  for uri in multi_https_test_server_fixture.getAllTestServerRootUris():
    nighthawk_client_args.append("--multi-target-endpoint")
    nighthawk_client_args.append(uri.replace("https://", "").replace("/", ""))

  parsed_json, stderr = multi_https_test_server_fixture.runNighthawkClient(nighthawk_client_args)

  counters = multi_https_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterEqual(counters, "benchmark.http_2xx", 25)
  assertCounterEqual(counters, "upstream_cx_http1_total", 3)
  assertCounterGreater(counters, "upstream_cx_rx_bytes_total", 0)
  assertCounterEqual(counters, "upstream_cx_total", 3)
  assertCounterGreater(counters, "upstream_cx_tx_bytes_total", 0)
  assertCounterEqual(counters, "upstream_rq_pending_total", 3)
  assertCounterEqual(counters, "upstream_rq_total", 25)
  assertCounterEqual(counters, "default.total_match_count", 3)
  total_2xx = 0
  for parsed_server_json in multi_https_test_server_fixture.getAllTestServerStatisticsJsons():
    single_2xx = multi_https_test_server_fixture.getServerStatFromJson(
        parsed_server_json, "http.ingress_http.downstream_rq_2xx")
    assertBetweenInclusive(single_2xx, 8, 9)
    total_2xx += single_2xx
  assertBetweenInclusive(total_2xx, 24, 25)


def test_https_h1_sni(sni_test_server_fixture):
  """
  Tests SNI indication works on https/h1
  """
  # Verify success when we set the right host
  parsed_json, _ = sni_test_server_fixture.runNighthawkClient([
      sni_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:2", "--request-header", "host: sni.com"
  ])
  counters = sni_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterGreaterEqual(counters, "benchmark.http_2xx", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_http1_total", 1)
  assertCounterGreaterEqual(counters, "ssl.handshake", 1)

  # Verify failure when we set no host (will get plain http)
  parsed_json, _ = sni_test_server_fixture.runNighthawkClient(
      [sni_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100"],
      expect_failure=True)

  # Verify success when we use plain http and don't request the sni host
  parsed_json, _ = sni_test_server_fixture.runNighthawkClient(
      [sni_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100"],
      expect_failure=True)

  parsed_json, _ = sni_test_server_fixture.runNighthawkClient([
      sni_test_server_fixture.getTestServerRootUri().replace("https://", "http://"), "--rps", "100",
      "--duration", "100", "--termination-predicate", "benchmark.http_2xx:2"
  ],
                                                              expect_failure=False)
  counters = sni_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterGreaterEqual(counters, "benchmark.http_2xx", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_http1_total", 1)
  assertNotIn("ssl.handshake", counters)


def test_https_h2_sni(sni_test_server_fixture):
  """
  Tests SNI indication works on https/h1
  """
  # Verify success when we set the right host
  parsed_json, _ = sni_test_server_fixture.runNighthawkClient([
      sni_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:2", "--request-header", ":authority: sni.com",
      "--h2"
  ])
  counters = sni_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterGreaterEqual(counters, "benchmark.http_2xx", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_http2_total", 1)
  assertCounterEqual(counters, "ssl.handshake", 1)

  # Verify success when we set the right host
  parsed_json, _ = sni_test_server_fixture.runNighthawkClient([
      sni_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:2", "--request-header", "host: sni.com", "--h2"
  ])
  counters = sni_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertCounterGreaterEqual(counters, "benchmark.http_2xx", 1)
  assertCounterGreaterEqual(counters, "upstream_cx_http2_total", 1)
  assertCounterEqual(counters, "ssl.handshake", 1)

  # Verify failure when we set no host (will get plain http)
  parsed_json, _ = sni_test_server_fixture.runNighthawkClient(
      [sni_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100", "--h2"],
      expect_failure=True)

  # Verify failure when we provide both host and :authority: (will get plain http)
  parsed_json, _ = sni_test_server_fixture.runNighthawkClient([
      sni_test_server_fixture.getTestServerRootUri(), "--rps", "100", "--duration", "100", "--h2",
      "--request-header", "host: sni.com", "--request-header", ":authority: sni.com"
  ],
                                                              expect_failure=True)


@pytest.fixture(scope="function", params=[1, 25])
def qps_parameterization_fixture(request):
  param = request.param
  yield param


@pytest.fixture(scope="function", params=[1, 3])
def duration_parameterization_fixture(request):
  param = request.param
  yield param


@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_request_release_timing(http_test_server_fixture, qps_parameterization_fixture,
                                     duration_parameterization_fixture):
  '''
  Verify latency-sample-, query- and reply- counts in various configurations.
  '''

  for concurrency in [1, 2]:
    parsed_json, _ = http_test_server_fixture.runNighthawkClient([
        http_test_server_fixture.getTestServerRootUri(), "--duration",
        str(duration_parameterization_fixture), "--rps",
        str(qps_parameterization_fixture), "--concurrency",
        str(concurrency)
    ])

    total_requests = qps_parameterization_fixture * concurrency * duration_parameterization_fixture
    global_histograms = http_test_server_fixture.getNighthawkGlobalHistogramsbyIdFromJson(
        parsed_json)
    counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
    assertEqual(
        int(global_histograms["benchmark_http_client.request_to_response"]["count"]),
        total_requests)
    assertEqual(
        int(global_histograms["benchmark_http_client.queue_to_connect"]["count"]), total_requests)

    # When it comes to qps/rps we also expect one warmup call per worker. We'll get rid
    # of this when we land the next part of the work with respect to phases.
    assertCounterEqual(counters, "benchmark.http_2xx", (total_requests) + concurrency)
