#!/usr/bin/env python3
"""@package integration_test.

Test with Endpoint churn with active traffic.
"""

import pytest
import os

from dynamic_config_envoy_proxy import (inject_dynamic_envoy_http_proxy_fixture, proxy_config)
from benchmarks import utilities as benchmarks_utilities
from typing import Generator
from rules_python.python.runfiles import runfiles
from nighthawk.api.configuration import endpoints_config_manager_pb2
from test.integration import utility

_BENCHMARK_DURATION = int(os.environ.get("NIGHTHAWK_BENCHMARK_DURATION", 30))


def _base_eds_config(
    temp_dir) -> endpoints_config_manager_pb2.DynamicEndpointsConfigManagerSettings:
  settings = endpoints_config_manager_pb2.DynamicEndpointsConfigManagerSettings()
  settings.refresh_interval.seconds = 5
  settings.output_file = os.path.join(temp_dir, 'new_eds.pb')
  return settings


def _run_benchmark(fixture,
                   rps=1000,
                   duration=_BENCHMARK_DURATION,
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

  parsed_json, _ = fixture.runNighthawkClient(args, check_return_code=False)

  # output test results
  benchmarks_utilities.output_benchmark_results(parsed_json, fixture)


def _readRunfile(path: str) -> str:
  runfiles_instance = runfiles.Create()
  with open(runfiles_instance.Rlocation(path)) as f:
    return f.read()


def _assignEndpointsToClusters(
    config: endpoints_config_manager_pb2.DynamicEndpointsConfigManagerSettings, cluster: str,
    available_endpoints: list[utility.SocketAddress]):
  """Assign the given backend endpoints to the cluster."""
  config.cluster.name = cluster
  for endpoint in available_endpoints:
    new_endpoint = config.cluster.endpoints.add()
    new_endpoint.ip = endpoint.ip
    new_endpoint.port = endpoint.port


def _config_generation_single_cluster(temp_dir: str, endpoints: list[utility.SocketAddress]):
  """Configure EDS churn for a single cluster."""
  config = _base_eds_config(temp_dir)
  clusters = 'service_envoyproxy_io'
  _assignEndpointsToClusters(config, clusters, endpoints)
  return config


@pytest.mark.parametrize('proxy_config',
                         ["nighthawk/benchmarks/configurations/dynamic_resources_eds.yaml"])
@pytest.mark.parametrize(
    'server_config',
    ["nighthawk/test/integration/configurations/nighthawk_15_listeners_http_origin.yaml"])
@pytest.mark.parametrize('dynamic_config_generator', [_config_generation_single_cluster])
def test_dynamic_http_single_cluster_traffic(inject_dynamic_envoy_http_proxy_fixture,
                                             proxy_config):  # noqa
  _run_benchmark(inject_dynamic_envoy_http_proxy_fixture)
