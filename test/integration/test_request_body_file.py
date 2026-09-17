"""Tests for --request-body-file."""

from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import asserts


def test_request_body_file_is_sent(http_test_server_fixture, tmp_path):
  """Send a binary body file over HTTP/1.1 and check the requests succeed."""
  payload = tmp_path / "body.bin"
  # Deliberately binary, with a NUL byte and non-UTF-8 content.
  payload.write_bytes(b"\x0a\x05world\x00\xff")
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--request-method", "POST",
      "--request-body-file",
      str(payload), "--request-header", "content-type: application/octet-stream", "--duration",
      "100", "--termination-predicate", "benchmark.http_2xx:9"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterEqual(counters, "benchmark.http_2xx", 10)
  asserts.assertCounterEqual(counters, "upstream_rq_total", 10)
