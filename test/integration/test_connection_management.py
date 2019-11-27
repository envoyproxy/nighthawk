#!/usr/bin/env python3

import logging
import os
import sys
import pytest

from test.integration.integration_test_fixtures import (http_test_server_fixture)
from test.integration.utility import *


def run_with_number_of_connections(fixture,
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
    assertCounterEqual(counters, "upstream_cx_total", expected_connections)
  return counters


# A series that tests with queueing disabled
@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_1(http_test_server_fixture):
  run_with_number_of_connections(http_test_server_fixture, 1)


@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_2(http_test_server_fixture):
  run_with_number_of_connections(http_test_server_fixture, 2)


# A series that tests with queueing enabled
@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_with_queue_1(http_test_server_fixture):
  run_with_number_of_connections(http_test_server_fixture, 1, max_pending_requests=5)


@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_with_queue_5(http_test_server_fixture):
  run_with_number_of_connections(http_test_server_fixture, 5, max_pending_requests=5)


def connection_management_test_request_per_connection(fixture, requests_per_connection, use_h2):
  max_requests_per_conn = 5
  counters = run_with_number_of_connections(
      fixture,
      1,
      max_pending_requests=1,
      requests_per_connection=max_requests_per_conn,
      run_test_expectation=False,
      h2=use_h2)
  requests = counters["upstream_rq_total"]
  assertCounterBetweenInclusive(counters, "upstream_cx_total", (requests / max_requests_per_conn),
                                (requests / max_requests_per_conn) + 1)


# Test h1 with a single request_per_connection
@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_single_request_per_conn_1(http_test_server_fixture):
  connection_management_test_request_per_connection(http_test_server_fixture, 1, False)


# Test h1 with a request_per_connection set to 5
@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h1_connection_management_single_request_per_conn_5(http_test_server_fixture):
  connection_management_test_request_per_connection(http_test_server_fixture, 5, False)


# Test h2 with a single request_per_connection
@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h2_connection_management_single_request_per_conn_1(http_test_server_fixture):
  connection_management_test_request_per_connection(http_test_server_fixture, 1, True)


# Test h2 with a request_per_connection set to 5
@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_http_h2_connection_management_single_request_per_conn_1(http_test_server_fixture):
  connection_management_test_request_per_connection(http_test_server_fixture, 5, True)


@pytest.mark.skipif(isSanitizerRun(), reason="Unstable in sanitizer runs")
def test_h1_pool_strategy(http_test_server_fixture):
  """
  Test that with the "HOT" strategy only the first created connection gets to send requests.
  Then, with the "FAIR" strategy, we expect the other connection to be used as well.
  """

  def countLogLinesWithSubstring(logs, substring):
    return len([line for line in logs.split(os.linesep) if substring in line])

  _, logs = http_test_server_fixture.runNighthawkClient([
      "--rps 20", "-v", "trace", "--connections", "2", "--prefetch-connections",
      "--h1-connection-reuse-strategy", "HOT", "--termination-predicate", "benchmark.http_2xx:10",
      http_test_server_fixture.getTestServerRootUri()
  ])

  requests = 60
  connections = 3
  assertNotIn("[C1] message complete", logs)
  assertEqual(countLogLinesWithSubstring(logs, "[C0] message complete"), 22)

  _, logs = http_test_server_fixture.runNighthawkClient([
      "--rps", "20", "-v trace", "--connections",
      str(connections), "--prefetch-connections", "--h1-connection-reuse-strategy", "FAIR",
      "--termination-predicate",
      "benchmark.http_2xx:%d" % requests,
      http_test_server_fixture.getTestServerRootUri()
  ])
  for i in range(1, connections):
    line_count = countLogLinesWithSubstring(logs, "[C%d] message complete" % i)
    strict_count = (requests / connections) * 2
    # We need to mind a single warmup call
    assertBetweenInclusive(line_count, strict_count, strict_count + 2)
