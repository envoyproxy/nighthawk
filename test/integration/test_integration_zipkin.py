"""Test the Zipkin tracing feature of Nighthawk's load generator."""

import pytest

# server_config needs to be explicitly imported to avoid an error, as http_test_server_fixture
# relies on it.
from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import asserts


def test_tracing_zipkin(http_test_server_fixture):
  """Test zipkin tracing.

  Test that we send spans when our zipkin tracing feature
  is enabled. Note there's no actual zipkin server started, so
  traffic will get (hopefully) get send into the void.
  """
  # TODO(https://github.com/envoyproxy/nighthawk/issues/141):
  # Boot up an actual zipkin server to accept spans we send here & validate based on that.
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--duration 5", "--termination-predicate", "benchmark.http_2xx:49", "--rps 100",
      "--trace zipkin://localhost:79/api/v2/spans",
      http_test_server_fixture.getTestServerRootUri()
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertGreaterEqual(counters["benchmark.http_2xx"], 50)
  asserts.assertGreaterEqual(counters["tracing.zipkin.reports_dropped"], 9)
  asserts.assertGreaterEqual(counters["tracing.zipkin.spans_sent"], 45)
