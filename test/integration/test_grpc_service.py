#!/usr/bin/env python3
import pytest

from integration_test_fixtures import (http_test_server_fixture)
from utility import *


def test_grpc_service(http_test_server_fixture):
  http_test_server_fixture.startNighthawkGrpcService()
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--duration 1", "--rps 10",
      "--header-source %s:%s" % (http_test_server_fixture.grpc_service.server_ip,
                                 http_test_server_fixture.grpc_service.server_port),
      http_test_server_fixture.getTestServerRootUri()
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  assertEqual(counters["benchmark.http_4xx"], 10)
  assertEqual(counters["headersource.internal.upstream_rq_200"], 1)
