#!/usr/bin/env python3
"""@package integration_test.

Test with Cluster churn with active traffic.
"""

import pytest

from dynamic_config_envoy_proxy import (dynamic_config_settings,
                                        inject_dynamic_envoy_http_proxy_fixture, proxy_config)
from benchmarks import utilities


def _run_benchmark(fixture,
                   rps=1000,
                   duration=30,
                   max_connections=1,
                   max_active_requests=100,
                   request_body_size=0,
                   response_size=1024,
                   concurrency=1):
  args = [
      fixture.getTestServerRootUri(), "--rps",
      str(rps), "--duration",
      str(duration), "--connections",
      str(max_connections), "--max-active-requests",
      str(max_active_requests), "--concurrency",
      str(concurrency), "--request-header",
      "x-nighthawk-test-server-config:{response_body_size:%s}" % response_size,
      "--experimental-h1-connection-reuse-strategy", "lru", "--prefetch-connections"
  ]
  if request_body_size > 0:
    args.append("--request-body-size")
    args.append(str(request_body_size))

  parsed_json, _ = fixture.runNighthawkClient(args, expect_failure=True)

  # output test results
  utilities.output_benchmark_results(parsed_json, fixture)


# Test via injected Envoy
@pytest.mark.parametrize('proxy_config',
                         ["nighthawk/benchmarks/configurations/dynamic_resources.yaml"])
@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_dynamic_http(inject_dynamic_envoy_http_proxy_fixture, proxy_config):  # noqa
  _run_benchmark(inject_dynamic_envoy_http_proxy_fixture)
