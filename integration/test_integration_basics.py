#!/usr/bin/env python3

import logging
import os
import sys
import unittest

from common import IpVersion
from integration_test_fixtures import (HttpIntegrationTestBase, HttpsIntegrationTestBase,
                                       IntegrationTestBase)

# TODO(oschaaf): rewrite the tests so we can just hand a map of expected key values to it.


class TestHttp(HttpIntegrationTestBase):

  def test_h1(self):
    parsed_json = self.runNighthawkClient([self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["benchmark.http_2xx"], 25)
    self.assertEqual(counters["upstream_cx_destroy"], 1)
    self.assertEqual(counters["upstream_cx_destroy_local"], 1)
    self.assertEqual(counters["upstream_cx_http1_total"], 1)
    self.assertEqual(counters["upstream_cx_rx_bytes_total"], 3400)
    self.assertEqual(counters["upstream_cx_total"], 1)
    self.assertEqual(counters["upstream_cx_tx_bytes_total"],
                     1400 if IntegrationTestBase.ip_version == IpVersion.IPV6 else 1500)
    self.assertEqual(counters["upstream_rq_pending_total"], 1)
    self.assertEqual(counters["upstream_rq_total"], 25)
    self.assertEqual(len(counters), 9)

  def test_h2(self):
    parsed_json = self.runNighthawkClient(["--h2", self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["benchmark.http_2xx"], 25)
    self.assertEqual(counters["upstream_cx_destroy"], 1)
    self.assertEqual(counters["upstream_cx_destroy_local"], 1)
    self.assertEqual(counters["upstream_cx_http2_total"], 1)
    self.assertGreaterEqual(counters["upstream_cx_rx_bytes_total"], 1145)
    self.assertEqual(counters["upstream_cx_total"], 1)
    self.assertGreaterEqual(counters["upstream_cx_tx_bytes_total"], 403)
    self.assertEqual(counters["upstream_rq_pending_total"], 1)
    self.assertEqual(counters["upstream_rq_total"], 25)
    self.assertEqual(len(counters), 9)


class TestHttps(HttpsIntegrationTestBase):

  def test_h1(self):
    parsed_json = self.runNighthawkClient([self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["benchmark.http_2xx"], 25)
    self.assertEqual(counters["upstream_cx_destroy"], 1)
    self.assertEqual(counters["upstream_cx_destroy_local"], 1)
    self.assertEqual(counters["upstream_cx_http1_total"], 1)
    self.assertEqual(counters["upstream_cx_rx_bytes_total"], 3400)
    self.assertEqual(counters["upstream_cx_total"], 1)
    self.assertEqual(counters["upstream_cx_tx_bytes_total"],
                     1400 if IntegrationTestBase.ip_version == IpVersion.IPV6 else 1500)
    self.assertEqual(counters["upstream_rq_pending_total"], 1)
    self.assertEqual(counters["upstream_rq_total"], 25)
    self.assertEqual(counters["ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256"], 1)
    self.assertEqual(counters["ssl.curves.X25519"], 1)
    self.assertEqual(counters["ssl.handshake"], 1)
    self.assertEqual(counters["ssl.sigalgs.rsa_pss_rsae_sha256"], 1)
    self.assertEqual(counters["ssl.versions.TLSv1.2"], 1)
    self.assertEqual(len(counters), 14)

  def test_h2(self):
    parsed_json = self.runNighthawkClient(["--h2", self.getTestServerRootUri()])
    counters = self.getNighthawkCounterMapFromJson(parsed_json)
    self.assertEqual(counters["benchmark.http_2xx"], 25)
    self.assertEqual(counters["upstream_cx_destroy"], 1)
    self.assertEqual(counters["upstream_cx_destroy_local"], 1)
    self.assertEqual(counters["upstream_cx_http2_total"], 1)
    self.assertGreaterEqual(counters["upstream_cx_rx_bytes_total"], 1145)
    self.assertEqual(counters["upstream_cx_total"], 1)
    self.assertGreaterEqual(counters["upstream_cx_tx_bytes_total"], 403)
    self.assertEqual(counters["upstream_rq_pending_total"], 1)
    self.assertEqual(counters["upstream_rq_total"], 25)
    self.assertEqual(counters["ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256"], 1)
    self.assertEqual(counters["ssl.curves.X25519"], 1)
    self.assertEqual(counters["ssl.handshake"], 1)
    self.assertEqual(counters["ssl.sigalgs.rsa_pss_rsae_sha256"], 1)
    self.assertEqual(counters["ssl.versions.TLSv1.2"], 1)
    self.assertEqual(len(counters), 14)
