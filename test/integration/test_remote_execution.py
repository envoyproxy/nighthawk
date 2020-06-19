#!/usr/bin/env python3

import pytest

from test.integration.integration_test_fixtures import http_test_server_fixture
from test.integration.utility import *


def test_remote_execution_basics(http_test_server_fixture):
  """
  Verify remote execution via gRPC works as intended. We do that by running
  nighthawk_service and configuring nighthawk_client to request execution via that.
  """
  http_test_server_fixture.startNighthawkGrpcService()
  args = [
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100", "--rps", "100",
      "--termination-predicate", "benchmark.http_2xx:24", "--nighthawk-service",
      "%s:%s" % (http_test_server_fixture.grpc_service.server_ip,
                 http_test_server_fixture.grpc_service.server_port)
  ]
  repeats = 3
  for i in range(repeats):
    parsed_json, _ = http_test_server_fixture.runNighthawkClient(args)
    counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
    assertCounterGreaterEqual(counters, "benchmark.http_2xx", 25)

  http_test_server_fixture.grpc_service.stop()
  # Ensure the gRPC service logs looks right. Specifically these logs ought to have sentinels
  # indicative of the right number of executions. (Avoids regression of #289).
  assertEqual(
      repeats,
      sum("Starting 1 threads / event loops" in line
          for line in http_test_server_fixture.grpc_service.log_lines))

  # As a control step, prove we are actually performing remote execution: re-run the command without an
  # operational gRPC service. That ought to fail.
  http_test_server_fixture.runNighthawkClient(args, expect_failure=True)


def test_bad_service_uri(http_test_server_fixture):
  """
  Test configuring a bad service uri.
  """
  args = [http_test_server_fixture.getTestServerRootUri(), "--nighthawk-service", "a:-1"]
  parsed_json, err = http_test_server_fixture.runNighthawkClient(
      args, expect_failure=True, as_json=False)
  assert "Bad service uri" in err
