#!/usr/bin/env python3
"""@package integration_test.

Test with Cluster churn with active traffic.
"""

import pytest

from dynamic_config_envoy_proxy import (dynamic_config_settings,
                                        inject_dynamic_envoy_http_proxy_fixture, proxy_config)
from benchmarks import utilities
from typing import Generator
from rules_python.python.runfiles import runfiles


def _run_benchmark(fixture,
                   rps=1000,
                   duration=30,
                   max_connections=1,
                   max_active_requests=100,
                   request_body_size=0,
                   response_size=1024,
                   concurrency=1,
                   request_source_config=None):
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

  if request_source_config:
    args.append("--request-source-plugin-config")
    args.append(_readRunfile(request_source_config))

  parsed_json, _ = fixture.runNighthawkClient(args, expect_failure=True)

  # output test results
  utilities.output_benchmark_results(parsed_json, fixture)


def _readRunfile(path: str) -> str:
  runfiles_instance = runfiles.Create()
  with open(runfiles_instance.Rlocation(path)) as f:
    return f.read()


# Test via injected Envoy
@pytest.mark.parametrize('proxy_config',
                         ["nighthawk/benchmarks/configurations/dynamic_resources.yaml"])
@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_dynamic_http_single_cluster_traffic(inject_dynamic_envoy_http_proxy_fixture,
                                             proxy_config):  # noqa
  _run_benchmark(inject_dynamic_envoy_http_proxy_fixture)


@pytest.fixture()
def request_source_config() -> Generator[str, None, None]:
  """Yield path to request_source_config for the nighthawk client to send different requests."""
  yield "nighthawk/benchmarks/configurations/request_source_five_clusters.json"


@pytest.mark.parametrize('proxy_config',
                         ["nighthawk/benchmarks/configurations/dynamic_resources.yaml"])
@pytest.mark.parametrize(
    'server_config',
    ["nighthawk/test/integration/configurations/nighthawk_15_listeners_http_origin.yaml"])
def test_dynamic_http_multiple_cluster_traffic(inject_dynamic_envoy_http_proxy_fixture,
                                               request_source_config, proxy_config):
  """Test that the nighthawkClient can run with request-source-plugin option."""
  _run_benchmark(inject_dynamic_envoy_http_proxy_fixture,
                 request_source_config=request_source_config)
