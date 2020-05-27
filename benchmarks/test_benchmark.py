#!/usr/bin/env python3

# NOTE: this is just a demo for now, to show how this works.

import logging
import json
import os
import pytest

from test.integration.common import IpVersion
from test.integration.integration_test_fixtures import (http_test_server_fixture,
                                                        https_test_server_fixture,
                                                        HttpIntegrationTestBase,
                                                        determineIpVersionsFromEnvironment)
from test.integration.utility import *
from test.integration.nighthawk_test_server import NighthawkTestServer


def run_with_cpu_profiler(fixture,
                          rps=999999,
                          use_h2=False,
                          duration=5,
                          max_connections=1,
                          max_active_requests=1,
                          request_body_size=0,
                          response_size=10,
                          concurrency=1):
  if hasattr(fixture, "proxy_server"):
    assert (fixture.proxy_server.enableCpuProfiler())
  assert (fixture.test_server.enableCpuProfiler())
  args = [
      fixture.getTestServerRootUri(), "--rps",
      str(rps), "--duration",
      str(duration), "--connections",
      str(max_connections), "--max-active-requests",
      str(max_active_requests), "--concurrency",
      str(concurrency), "--request-header",
      "x-nighthawk-test-server-config:{response_body_size:%s}" % response_size
  ]
  if use_h2:
    args.append("--h2")
  if request_body_size > 0:
    args.append("--request-body-size")
    args.append(str(request_body_size))

  parsed_json, _ = fixture.runNighthawkClient(args)
  counters = fixture.getNighthawkCounterMapFromJson(parsed_json)
  response_count = counters["benchmark.http_2xx"]
  request_count = counters["upstream_rq_total"]
  connection_counter = "upstream_cx_http2_total" if use_h2 else "upstream_cx_http1_total"

  # Some arbitrary sanity checks 
  assertCounterGreater(counters, "benchmark.http_2xx", 1000)
  assertGreater(counters["upstream_cx_rx_bytes_total"], response_count * response_size)
  assertGreater(counters["upstream_cx_tx_bytes_total"], request_count * request_body_size)
  assertCounterEqual(counters, connection_counter, max_connections)

  # Could potentially set thresholds on acceptable latency here.

  # dump output
  logging.info(fixture.transformNighthawkJson(json.dumps(parsed_json)))


class EnvoyProxyServer(NighthawkTestServer):
  def __init__(self,
               server_binary_path,
               config_template_path,
               server_ip,
               ip_version,
               parameters=dict(),
               tag=""):
    super(EnvoyProxyServer, self).__init__(server_binary_path, config_template_path, server_ip,
                                              ip_version, parameters=parameters, tag=tag)
    self.docker_image = os.getenv("ENVOY_DOCKER_IMAGE_TO_TEST")

class InjectHttpProxyIntegrationTestBase(HttpIntegrationTestBase):
  """
  Base for running plain http tests against the Nighthawk test server
  """

  def __init__(self, ip_version):
    """See base class."""
    super(InjectHttpProxyIntegrationTestBase, self).__init__(ip_version)

  def setUp(self):
    super(InjectHttpProxyIntegrationTestBase, self).setUp()
    logging.info("injecting envoy proxy ...")
    # TODO(oschaaf): how should this interact with multiple backends?
    self.parameters["proxy_ip"] = self.test_server.server_ip
    self.parameters["server_port"] = self.test_server.server_port
    proxy_server = EnvoyProxyServer("envoy",
                                    "benchmarks/configurations/envoy_proxy.yaml", self.server_ip,
                                    self.ip_version, parameters=self.parameters, tag=self.tag)
    assert (proxy_server.start())
    logging.info("envoy proxy listening at {ip}:{port}".format(ip=proxy_server.server_ip, port=proxy_server.server_port))
    self.proxy_server = proxy_server

  def getTestServerRootUri(self):
    """See base class."""
    r = super(InjectHttpProxyIntegrationTestBase, self).getTestServerRootUri()
    # TODO(oschaaf): fix, kind of a hack.
    r = r.replace(":%s" % self.test_server.server_port, ":%s" % self.proxy_server.server_port)
    return r

@pytest.fixture(params=determineIpVersionsFromEnvironment())
def inject_envoy_http_proxy_fixture(request):
  '''
  Injects an Envoy proxy.
  '''
  f = InjectHttpProxyIntegrationTestBase(request.param)
  f.setUp()
  yield f
  f.tearDown()


# Plain http: baseline vs running via Envoy
def test_http_h1_small_request_small_reply_via(inject_envoy_http_proxy_fixture):
  run_with_cpu_profiler(inject_envoy_http_proxy_fixture)

def test_http_h1_small_request_small_reply_direct(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture)

