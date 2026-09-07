"""Tests for gRPC unary mode (--grpc-mode unary)."""

import pytest

from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import asserts

GRPC_STATUS_OK_CONFIG = ("x-nighthawk-test-server-config: "
                         "{v3_response_headers:[{header:{key:\"grpc-status\",value:\"0\"}}]}")
GRPC_STATUS_INTERNAL_CONFIG = (
    "x-nighthawk-test-server-config: "
    "{v3_response_headers:[{header:{key:\"grpc-status\",value:\"13\"}}]}")


def _write_payload(tmp_path):
  # Deliberately binary, with a NUL byte and non-UTF-8 content.
  payload = tmp_path / "hello.pb"
  payload.write_bytes(b"\x0a\x05world\x00\xff")
  return str(payload)


def test_grpc_unary_ok(http_test_server_fixture, tmp_path):
  """A grpc-status 0 response is a success: http_2xx, grpc_status.0 and latency_grpc_ok."""
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri() + "acme.greeter.Greeter/SayHello", "--grpc-mode", "unary",
      "--request-body-file",
      _write_payload(tmp_path), "--request-header", GRPC_STATUS_OK_CONFIG, "--duration", "100",
      "--termination-predicate", "benchmark.http_2xx:9"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterEqual(counters, "benchmark.http_2xx", 10)
  asserts.assertCounterEqual(counters, "benchmark.grpc_status.0", 10)
  asserts.assertNotIn("benchmark.grpc_error", counters)
  asserts.assertCounterGreaterEqual(counters, "upstream_cx_http2_total", 1)
  global_histograms = http_test_server_fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
  asserts.assertEqual(int(global_histograms["benchmark_http_client.latency_grpc_ok"]["count"]), 10)
  asserts.assertEqual(int(global_histograms["benchmark_http_client.latency_2xx"]["count"]), 10)


def test_grpc_unary_error_is_not_a_2xx_success(http_test_server_fixture, tmp_path):
  """HTTP 200 with grpc-status 13 must count as grpc_error, not as http_2xx."""
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri() + "acme.greeter.Greeter/SayHello", "--grpc-mode", "unary",
      "--request-body-file",
      _write_payload(tmp_path), "--request-header", GRPC_STATUS_INTERNAL_CONFIG, "--duration",
      "100", "--termination-predicate", "benchmark.grpc_error:9", "--no-default-failure-predicates"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterEqual(counters, "benchmark.grpc_error", 10)
  asserts.assertCounterEqual(counters, "benchmark.grpc_status.13", 10)
  asserts.assertNotIn("benchmark.http_2xx", counters)
  global_histograms = http_test_server_fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
  asserts.assertEqual(int(global_histograms["benchmark_http_client.latency_grpc_ok"]["count"]), 0)


def test_grpc_unary_synthesized_status_is_an_error(http_test_server_fixture, tmp_path):
  """A gRPC request answered by a non-gRPC origin is a failed call.

  The test server is an Envoy; it recognizes the gRPC request and rewrites its local reply into
  a gRPC response with grpc-status 2 (UNKNOWN), which must be scored as an error.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri() + "acme.greeter.Greeter/SayHello", "--grpc-mode", "unary",
      "--request-body-file",
      _write_payload(tmp_path), "--duration", "100", "--termination-predicate",
      "benchmark.grpc_error:9", "--no-default-failure-predicates"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterEqual(counters, "benchmark.grpc_error", 10)
  asserts.assertCounterEqual(counters, "benchmark.grpc_status.2", 10)
  asserts.assertNotIn("benchmark.http_2xx", counters)


def test_grpc_unary_missing_status_is_an_error(http_test_server_fixture, tmp_path):
  """A plain HTTP 200 without any grpc-status is a failed call in gRPC mode.

  Overriding the content-type keeps the test server from treating the request as gRPC, so the
  response carries no grpc-status at all.
  """
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri() + "acme.greeter.Greeter/SayHello", "--grpc-mode", "unary",
      "--request-body-file",
      _write_payload(tmp_path), "--request-header", "content-type: application/octet-stream",
      "--duration", "100", "--termination-predicate", "benchmark.grpc_error:9",
      "--no-default-failure-predicates"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterEqual(counters, "benchmark.grpc_error", 10)
  asserts.assertCounterEqual(counters, "benchmark.grpc_status.missing", 10)
  asserts.assertNotIn("benchmark.http_2xx", counters)
