"""Test connection management of nighthawk's load generator."""

import logging
import os
import sys
import pytest

from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import asserts
from test.integration import utility


def _run_with_number_of_connections(fixture,
                                    number_of_connections,
                                    expected_connections=-1,
                                    max_pending_requests=0,
                                    requests_per_connection=1024 * 1024,
                                    rps=100,
                                    run_test_expectation=True,
                                    h2=False):
  if expected_connections == -1:
    expected_connections = number_of_connections
  # We add a delay to responses to make sure connections are needed, as the pool creates connections on-demand.
  args = [
      fixture.getTestServerRootUri(), "--rps",
      str(rps), "--duration", "2", "--request-header", "x-envoy-fault-delay-request:500",
      "--max-pending-requests",
      str(max_pending_requests), "--max-requests-per-connection",
      str(requests_per_connection), "--connections",
      str(number_of_connections)
  ]
  if h2:
    args.append("--h2")
  parsed_json, _ = fixture.runNighthawkClient(args)
  counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
  if run_test_expectation:
    asserts.assertCounterEqual(counters, "upstream_cx_total", expected_connections)
  return counters


# A series that tests with queueing disabled
@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_h1_pool_strategy_lru(http_test_server_fixture):
  """Test connection re-use strategies of the http 1 connection pool.

  Test that with the "least recently used" (lru) strategy all connections are used with roughly equal distribution.
  """
  requests = 12
  connections = 3
  _, logs = http_test_server_fixture.runNighthawkClient([
      "--rps",
      "5",
      "-v trace",
      "--duration",
      "20",
      "--connections",
      str(connections),
      "--prefetch-connections",
      "--experimental-h1-connection-reuse-strategy",
      "lru",
      "--termination-predicate",
      # termination-predicate takes affect when it exceeds the limit. Therefore, set the limit to 1 less than the desired number of requests.
      "benchmark.http_2xx:%d" % (requests - 1),
      http_test_server_fixture.getTestServerRootUri()
  ])

  line_counts = []
  for i in range(1, connections):
    line_counts.append(
        float(utility.count_log_lines_with_substring(logs, "[C%d] message complete" % i)))

  average_line_count = sum(line_counts) / len(line_counts)
  for line_count in line_counts:
    asserts.assertBetweenInclusive(
        line_count,
        # provide a little slack. Minimum of 2 as envoy logs "message complete" twice per request.
        average_line_count - 2,
        average_line_count + 2)
