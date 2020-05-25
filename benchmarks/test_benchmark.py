#!/usr/bin/env python3

import logging
import json
import os
import sys
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
  # TODO(oschaaf): refactor
  if hasattr(fixture, "proxy_server"):
    assert (fixture.proxy_server.enableCpuProfiler())
  MIN_EXPECTED_REQUESTS = 100
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
  # We expect to have executed a certain amount of requests
  assertCounterGreater(counters, "benchmark.http_2xx", MIN_EXPECTED_REQUESTS)
  response_count = counters["benchmark.http_2xx"]
  assertGreater(counters["upstream_cx_rx_bytes_total"], response_count * response_size)

  request_count = counters["upstream_rq_total"]
  # TODO(oschaaf): There's something weird here, the numbers don't add up. We divide by as a temp workaround
  # to pass here, but this surely deserves investigation.
  # Note: numbers are even below what we would expect when considering the number of confirmed replies.
  assertGreater(counters["upstream_cx_tx_bytes_total"], (request_count * request_body_size) / 10)

  # We expect to have created only a single connection
  if use_h2:
    assertCounterEqual(counters, "upstream_cx_http2_total", 1)
  else:
    # Apparently, when a request_body_size > 0 is involved, we will create > 1 connections.
    # TODO(oschaaf): figure out the specifics of ^^.
    if request_body_size == 0:
      assertCounterEqual(counters, "upstream_cx_http1_total", 1)

  global_histograms = fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
  assertGreater(int(global_histograms["sequencer.blocking"]["count"]), MIN_EXPECTED_REQUESTS)
  assertGreater(
      int(global_histograms["benchmark_http_client.request_to_response"]["count"]),
      MIN_EXPECTED_REQUESTS)
  # dump output
  logging.info(fixture.transformNighthawkJsonToHumanReadable(json.dumps(parsed_json)))


class EnvoyProxyServer(NighthawkTestServer):
  def __init__(self,
               server_binary_path,
               config_template_path,
               server_ip,
               ip_version,
               parameters=dict()):
    super(EnvoyProxyServer, self).__init__(server_binary_path, config_template_path, server_ip,
                                              ip_version, parameters)
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
                                    self.ip_version, self.parameters)
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

#def test_http_h1_small_request_small_reply_direct(http_test_server_fixture):
#  run_with_cpu_profiler(http_test_server_fixture)

# Some more samples. These don't run via an injected Envoy (yet).
def test_https_h1_small_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture)


def test_http_h2_small_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, use_h2=True)


def test_https_h2_small_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, use_h2=True)


# TODO(oschaaf): With 1MB request body sizes we hit a threshold, which triggers a panic. I suspect this is because of our
# custom streamdecoder asserting on some unimplemented watermark callbacks.
def test_http_h1_1mb_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000)


def test_https_h1_1mb_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000)


def test_http_h2_1mb_request_small_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


def test_https_h2_1mb_request_small_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


# A series with ~1MB request/replies
def test_http_h1_1mb_request_1MB_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000)


def test_https_h1_1mb_request_1MB_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000)


def test_http_h2_1mb_request_1MB_reply(http_test_server_fixture):
  run_with_cpu_profiler(http_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


def test_https_h2_1mb_request_1MB_reply(https_test_server_fixture):
  run_with_cpu_profiler(https_test_server_fixture, request_body_size=1000 * 1000, use_h2=True)


# TODO: add tests using multiple cores on both front and backend.
# TODO: the current Envoy is tested in direct response mode. add an env where we run this with Envoy as a proxy in the middle.
