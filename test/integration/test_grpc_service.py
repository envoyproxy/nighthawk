#!/usr/bin/env python3
import pytest

from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import utility


def test_grpc_service_happy_flow(http_test_server_fixture):
  http_test_server_fixture.startNighthawkGrpcService("dummy-request-source")
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--termination-predicate", "benchmark.http_2xx:5", "--rps 10",
      "--request-source %s:%s" % (http_test_server_fixture.grpc_service.server_ip,
                                  http_test_server_fixture.grpc_service.server_port),
      http_test_server_fixture.getTestServerRootUri()
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  utility.assertGreaterEqual(counters["benchmark.http_2xx"], 5)
  utility.assertEqual(counters["requestsource.internal.upstream_rq_200"], 1)


def test_grpc_service_down(http_test_server_fixture):
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--rps 100",
      "--request-source %s:%s" % (http_test_server_fixture.server_ip, "34589"),
      http_test_server_fixture.getTestServerRootUri()
  ],
                                                               expect_failure=True)
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  utility.assertEqual(counters["requestsource.upstream_rq_pending_failure_eject"], 1)


@pytest.mark.skipif(utility.isSanitizerRun(), reason="Slow in sanitizer runs")
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
  utility.assertGreaterEqual(counters["benchmark.http_2xx"], 5000)
  utility.assertEqual(counters["requestsource.internal.upstream_rq_200"], 4)


def _run_service_with_args(args):
  return utility.run_binary_with_args("nighthawk_service", args)


def test_grpc_service_help():
  (exit_code, output) = _run_service_with_args("--help")
  utility.assertEqual(exit_code, 0)
  utility.assertIn("USAGE", output)


def test_grpc_service_bad_arguments():
  (exit_code, output) = _run_service_with_args("--foo")
  utility.assertEqual(exit_code, 1)
  utility.assertIn("PARSE ERROR: Argument: --foo", output)


def test_grpc_service_nonexisting_listener_address():
  (exit_code, output) = _run_service_with_args("--listen 1.1.1.1:1")
  utility.assertEqual(exit_code, 1)
  utility.assertIn("Failure: Could not start the grpc service", output)
