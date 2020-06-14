#!/usr/bin/env python3
import pytest

from test.integration.integration_test_fixtures import http_test_server_fixture
from test.integration.utility import *


def test_grpc_service_happy_flow(http_test_server_fixture):
  http_test_server_fixture.startNighthawkGrpcService("dummy-request-source")
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--termination-predicate", "benchmark.http_2xx:5", "--rps 10",
      "--request-source %s:%s" % (http_test_server_fixture.grpc_service.server_ip,
                                  http_test_server_fixture.grpc_service.server_port),
      http_test_server_fixture.getTestServerRootUri()
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertGreaterEqual(counters["benchmark.http_2xx"], 5)
  assertEqual(counters["requestsource.internal.upstream_rq_200"], 1)


def test_grpc_service_down(http_test_server_fixture):
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--rps 100",
      "--request-source %s:%s" % (http_test_server_fixture.server_ip, "34589"),
      http_test_server_fixture.getTestServerRootUri()
  ],
                                                               expect_failure=True)
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertEqual(counters["requestsource.upstream_rq_pending_failure_eject"], 1)


@pytest.mark.skipif(isSanitizerRun(), reason="Slow in sanitizer runs")
def test_grpc_service_stress(http_test_server_fixture):
  http_test_server_fixture.startNighthawkGrpcService("dummy-request-source")
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--duration 100", "--rps 10000", "--concurrency 4", "--termination-predicate",
      "benchmark.http_2xx:5000",
      "--request-source %s:%s" % (http_test_server_fixture.grpc_service.server_ip,
                                  http_test_server_fixture.grpc_service.server_port),
      http_test_server_fixture.getTestServerRootUri()
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertGreaterEqual(counters["benchmark.http_2xx"], 5000)
  assertEqual(counters["requestsource.internal.upstream_rq_200"], 4)


def run_service_with_args(args):
  return run_binary_with_args("nighthawk_service", args)


def test_grpc_service_help():
  (exit_code, output) = run_service_with_args("--help")
  assert (exit_code == 0)
  assert ("USAGE" in output)


def test_grpc_service_bad_arguments():
  (exit_code, output) = run_service_with_args("--foo")
  assert (exit_code == 1)
  assert ("PARSE ERROR: Argument: --foo" in output)


def test_grpc_service_nonexisting_listener_address():
  (exit_code, output) = run_service_with_args("--listen 1.1.1.1:1")
  assert (exit_code == 1)
  assert ("Failure: Could not start the grpc service" in output)
