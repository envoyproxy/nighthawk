#!/usr/bin/env python3

import logging
import os
import sys
import unittest

from common import IpVersion
from integration_test_fixtures import (HttpIntegrationTestBase, HttpsIntegrationTestBase,
                                       IntegrationTestBase)

# TODO(oschaaf): we mostly verify stats observed from the client-side. Add expectations
# for the server side as well.


class TestHttp(HttpIntegrationTestBase):

  def test_h1(self):
    """
    Runs the CLI configured to use plain HTTP/1 against our test server, and sanity
    checks statistics from both client and server.
    """
    parsed_json, _ = self.runNighthawkClient([self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertIsSubset({
        "benchmark.http_2xx": 25,
        "upstream_cx_destroy": 1,
        "upstream_cx_destroy_local": 1,
        "upstream_cx_http1_total": 1,
        "upstream_cx_rx_bytes_total": 3400,
        "upstream_cx_total": 1,
        "upstream_rq_pending_total": 1,
        "upstream_rq_total": 25,
    }, counters)
    self.assertEqual(counters["upstream_cx_tx_bytes_total"],
                     1400 if IntegrationTestBase.ip_version == IpVersion.IPV6 else 1500)
    self.assertEqual(len(counters), 9)

  def mini_stress_test_h1(self, args):
    # run a test with more rps then we can handle, and a very small client-side queue.
    # we should observe both lots of successfull requests as well as time spend in blocking mode.,
    parsed_json, _ = self.runNighthawkClient(args)
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    # We set a reasonably low expecation of 100 requests. We set it low, because we want this
    # test to succeed on a reasonable share of setups (hopefully practically all).
    MIN_EXPECTED_REQUESTS = 100
    self.assertGreater(counters["benchmark.http_2xx"], MIN_EXPECTED_REQUESTS)
    self.assertEqual(counters["upstream_cx_http1_total"], 1)
    global_histograms = self.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
    self.assertGreater(int(global_histograms["sequencer.blocking"]["count"]), MIN_EXPECTED_REQUESTS)
    self.assertGreater(
        int(global_histograms["benchmark_http_client.request_to_response"]["count"]),
        MIN_EXPECTED_REQUESTS)
    return counters

  def test_h1_mini_stress_test_with_client_side_queueing(self):
    """
    Run a max rps test with the h1 pool against our test server, using a small client-side
    queue. We expect to observe:
    - upstream_rq_pending_total increasing 
    - upstream_cx_overflow overflows 
    - blocking to be reported by the sequencer
    """
    counters = self.mini_stress_test_h1([
        self.getTestServerRootUri(), "--rps", "999999", "--max-pending-requests", "10",
        "--duration 2"
    ])
    self.assertGreater(counters["upstream_rq_pending_total"], 100)
    self.assertGreater(counters["upstream_cx_overflow"], 0)

  def test_h1_mini_stress_test_without_client_side_queueing(self):
    """
    Run a max rps test with the h1 pool against our test server, with no client-side
    queueing. We expect to observe:
    - upstream_rq_pending_total to be equal to 1
    - blocking to be reported by the sequencer
    - no upstream_cx_overflows
    """
    counters = self.mini_stress_test_h1(
        [self.getTestServerRootUri(), "--rps", "999999", "--duration 2"])
    self.assertEqual(counters["upstream_rq_pending_total"], 1)
    self.assertFalse("upstream_cx_overflow" in counters)

  def test_h2(self):
    """
    Runs the CLI configured to use h2c against our test server, and sanity
    checks statistics from both client and server.
    """
    parsed_json, _ = self.runNighthawkClient(["--h2", self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertIsSubset({
        "benchmark.http_2xx": 25,
        "upstream_cx_destroy": 1,
        "upstream_cx_destroy_local": 1,
        "upstream_cx_http2_total": 1,
        "upstream_cx_total": 1,
        "upstream_rq_pending_total": 1,
        "upstream_rq_total": 25,
    }, counters)

    self.assertGreaterEqual(counters["upstream_cx_rx_bytes_total"], 1145)
    self.assertGreaterEqual(counters["upstream_cx_tx_bytes_total"], 403)
    self.assertEqual(len(counters), 9)

  def test_concurrency(self):
    """
    Concurrency should act like a multiplier.
    """

    parsed_json, _ = self.runNighthawkClient(
        ["--concurrency 4 --rps 5 --connections 1",
         self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)

    self.assertGreater(counters["benchmark.http_2xx"], 25)
    self.assertLessEqual(counters["benchmark.http_2xx"], 100)
    self.assertEqual(counters["upstream_cx_http1_total"], 4)


class TestHttps(HttpsIntegrationTestBase):

  def test_h1(self):
    """
    Runs the CLI configured to use HTTP/1 over https against our test server, and sanity
    checks statistics from both client and server.
    """
    parsed_json, _ = self.runNighthawkClient([self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertIsSubset({
        "benchmark.http_2xx": 25,
        "upstream_cx_destroy": 1,
        "upstream_cx_destroy_local": 1,
        "upstream_cx_http1_total": 1,
        "upstream_cx_rx_bytes_total": 3400,
        "upstream_cx_total": 1,
        "upstream_rq_pending_total": 1,
        "upstream_rq_total": 25,
        "ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256": 1,
        "ssl.curves.X25519": 1,
        "ssl.handshake": 1,
        "ssl.sigalgs.rsa_pss_rsae_sha256": 1,
        "ssl.versions.TLSv1.2": 1,
    }, counters)
    self.assertEqual(counters["upstream_cx_tx_bytes_total"],
                     1400 if IntegrationTestBase.ip_version == IpVersion.IPV6 else 1500)
    self.assertEqual(len(counters), 14)

    server_stats = self.getTestServerStatisticsJson()
    self.assertEqual(
        self.getServerStatFromJson(server_stats, "http.ingress_http.downstream_rq_2xx"), 25)

  def test_h2(self):
    """
    Runs the CLI configured to use HTTP/2 (using https) against our test server, and sanity
    checks statistics from both client and server.
    """

    parsed_json, _ = self.runNighthawkClient(["--h2", self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertIsSubset({
        "benchmark.http_2xx": 25,
        "upstream_cx_destroy": 1,
        "upstream_cx_destroy_local": 1,
        "upstream_cx_http2_total": 1,
        "upstream_cx_total": 1,
        "upstream_rq_pending_total": 1,
        "upstream_rq_total": 25,
        "ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256": 1,
        "ssl.curves.X25519": 1,
        "ssl.handshake": 1,
        "ssl.sigalgs.rsa_pss_rsae_sha256": 1,
        "ssl.versions.TLSv1.2": 1
    }, counters)
    self.assertGreaterEqual(counters["upstream_cx_rx_bytes_total"], 1145)
    self.assertGreaterEqual(counters["upstream_cx_tx_bytes_total"], 403)
    self.assertEqual(len(counters), 14)

  def test_h1_tls_context_configuration(self):
    """
    Verifies specifying tls cipher suites works with the h1 pool
    """

    parsed_json, _ = self.runNighthawkClient([
        "--duration 1",
        "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES128-SHA\"]}}}",
        self.getTestServerRootUri()
    ])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["ssl.ciphers.ECDHE-RSA-AES128-SHA"], 1)

    parsed_json, _ = self.runNighthawkClient([
        "--h2", "--duration 1",
        "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"]}}}",
        self.getTestServerRootUri()
    ])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["ssl.ciphers.ECDHE-RSA-AES256-GCM-SHA384"], 1)

  def test_h2_tls_context_configuration(self):
    """
    Verifies specifying tls cipher suites works with the h2 pool
    """
    parsed_json, _ = self.runNighthawkClient([
        "--duration 1",
        "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES128-SHA\"]}}}",
        self.getTestServerRootUri()
    ])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["ssl.ciphers.ECDHE-RSA-AES128-SHA"], 1)

    parsed_json, _ = self.runNighthawkClient([
        "--h2", "--duration 1",
        "--tls-context {common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"]}}}",
        self.getTestServerRootUri()
    ])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["ssl.ciphers.ECDHE-RSA-AES256-GCM-SHA384"], 1)

  def test_prefetching(self):
    """
    Test we prefetch connections. We test for 1 second at 1 rps, which should
    result in 1 connection max without prefetching. However, we specify 50 connections
    and the prefetching flag, so we ought to see 50 http1 connections created.
    """
    parsed_json, _ = self.runNighthawkClient([
        "--duration 1", "--rps 1", "--prefetch-connections", "--connections 50",
        self.getTestServerRootUri()
    ])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["upstream_cx_http1_total"], 50)

  def test_log_verbosity(self):
    """
    Test that that the specified log verbosity level is respected.
    This tests for a sentinel we know is only right when the level
    is set to 'trace'.
    """
    # TODO(oschaaf): this is kind of fragile. Can we improve?
    trace_level_sentinel = "nighthawk_service_zone"
    parsed_json, logs = self.runNighthawkClient(
        ["--duration 1", "--rps 1", "-v debug",
         self.getTestServerRootUri()])
    self.assertNotIn(trace_level_sentinel, logs)

    parsed_json, logs = self.runNighthawkClient(
        ["--duration 1", "--rps 1", "-v trace",
         self.getTestServerRootUri()])
    self.assertIn(trace_level_sentinel, logs)
