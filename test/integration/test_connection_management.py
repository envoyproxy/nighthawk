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
def test_http_h1_connection_management_1(http_test_server_fixture):
  """Test http h1 connection management with 1 connection and queueing disabled."""
  _run_with_number_of_connections(http_test_server_fixture, 1)


@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_2(http_test_server_fixture):
  """Test http h1 connection management with 2 connections and queueing disabled."""
  _run_with_number_of_connections(http_test_server_fixture, 2, run_test_expectation=False)


# A series that tests with queueing enabled
@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_with_queue_1(http_test_server_fixture):
  """Test http h1 connection management with 1 connection and queueing enabled."""
  _run_with_number_of_connections(http_test_server_fixture, 1, max_pending_requests=5)


@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_with_queue_5(http_test_server_fixture):
  """Test http h1 connection management with 5 connections and queueing enabled."""
  _run_with_number_of_connections(http_test_server_fixture, 5, max_pending_requests=5)


def _connection_management_test_request_per_connection(fixture, requests_per_connection, use_h2):
  max_requests_per_conn = 5
  counters = _run_with_number_of_connections(fixture,
                                             1,
                                             max_pending_requests=1,
                                             requests_per_connection=max_requests_per_conn,
                                             run_test_expectation=False,
                                             h2=use_h2)
  requests = counters["upstream_rq_total"]
  asserts.assertCounterBetweenInclusive(counters, "upstream_cx_total",
                                        (requests / max_requests_per_conn),
                                        (requests / max_requests_per_conn) + 1)


@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_single_request_per_conn_1(http_test_server_fixture):
  """Test h1 with a single request_per_connection."""
  _connection_management_test_request_per_connection(http_test_server_fixture, 1, False)


@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_single_request_per_conn_5(http_test_server_fixture):
  """Test h1 with a request_per_connection set to 5."""
  _connection_management_test_request_per_connection(http_test_server_fixture, 5, False)


@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h2_connection_management_single_request_per_conn_1(http_test_server_fixture):
  """Test h2 with a single request_per_connection."""
  _connection_management_test_request_per_connection(http_test_server_fixture, 1, True)


@pytest.mark.skipif(utility.isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h2_connection_management_single_request_per_conn_1(http_test_server_fixture):
  """Test h2 with a request_per_connection set to 5."""
  _connection_management_test_request_per_connection(http_test_server_fixture, 5, True)


def test_h1_pool_strategy(http_test_server_fixture):
  """Test connection re-use strategies of the http 1 connection pool.

  Test that with the "mru" strategy only the first created connection gets to send requests.
  Then, with the "lru" strategy, we expect the other connection to be used as well.
  """

  def countLogLinesWithSubstring(logs, substring):
    return len([line for line in logs.split(os.linesep) if substring in line])

  _, logs = http_test_server_fixture.runNighthawkClient([
      "--rps 5", "-v", "trace", "--connections", "2", "--prefetch-connections",
      "--experimental-h1-connection-reuse-strategy", "mru", "--termination-predicate",
      "benchmark.http_2xx:4",
      http_test_server_fixture.getTestServerRootUri()
  ])

  asserts.assertNotIn("[C1] message complete", logs)
  asserts.assertEqual(countLogLinesWithSubstring(logs, "[C0] message complete"), 10)

  requests = 12
  connections = 3
  _, logs = http_test_server_fixture.runNighthawkClient([
      "--rps", "5", "-v trace", "--connections",
      str(connections), "--prefetch-connections", "--experimental-h1-connection-reuse-strategy",
      "lru", "--termination-predicate",
      "benchmark.http_2xx:%d" % (requests - 1),
      http_test_server_fixture.getTestServerRootUri()
  ])
  for i in range(1, connections):
    line_count = countLogLinesWithSubstring(logs, "[C%d] message complete" % i)
    strict_count = (requests / connections) * 2
    asserts.assertBetweenInclusive(line_count, strict_count, strict_count)
