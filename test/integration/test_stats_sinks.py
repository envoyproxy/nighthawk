"""Tests for the statsd stats sink (--stats-sinks)."""

import socket
import threading

from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import asserts
from test.integration.common import IpVersion


class UdpCollector:
  """Collects statsd datagrams on a loopback UDP socket in a background thread."""

  def __init__(self, ip_version):
    """Bind a UDP socket on the loopback address for ip_version and start receiving."""
    family = socket.AF_INET6 if ip_version == IpVersion.IPV6 else socket.AF_INET
    self.address = "::1" if ip_version == IpVersion.IPV6 else "127.0.0.1"
    self._socket = socket.socket(family, socket.SOCK_DGRAM)
    self._socket.bind((self.address, 0))
    self._socket.settimeout(0.2)
    self.port = self._socket.getsockname()[1]
    self.lines = []
    self._stop = False
    self._thread = threading.Thread(target=self._run, daemon=True)
    self._thread.start()

  def _run(self):
    while not self._stop:
      try:
        data, _ = self._socket.recvfrom(65536)
      except socket.timeout:
        continue
      self.lines.extend(data.decode("utf-8").split("\n"))

  def stop(self):
    """Stop the receiver thread and close the socket."""
    self._stop = True
    self._thread.join()
    self._socket.close()


def _sink_config(name, type_name, address, port):
  return ("{name:\"%s\",typed_config:{\"@type\":\"type.googleapis.com/"
          "envoy.config.metrics.v3.%s\",address:{socket_address:{address:\"%s\","
          "port_value:%d}}}}") % (name, type_name, address, port)


def test_statsd_sink_receives_counters_and_latencies(http_test_server_fixture):
  """Run with a statsd sink and check counters and latency timings arrive over UDP."""
  collector = UdpCollector(http_test_server_fixture.ip_version)
  try:
    parsed_json, _ = http_test_server_fixture.runNighthawkClient([
        http_test_server_fixture.getTestServerRootUri(), "--rps", "50", "--duration", "3",
        "--stats-flush-interval", "1", "--stats-sinks",
        _sink_config("envoy.stat_sinks.statsd", "StatsdSink", collector.address, collector.port)
    ])
  finally:
    collector.stop()
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterGreaterEqual(counters, "benchmark.http_2xx", 100)

  counter_lines = [
      line for line in collector.lines if line.startswith("nighthawk.worker.0.benchmark.http_2xx:")
  ]
  asserts.assertGreaterEqual(len(counter_lines), 1)
  for line in counter_lines:
    asserts.assertIn("|c", line)
  # Counter deltas across flushes add up to (at most) the final counter value.
  total = sum(int(line.split(":")[1].split("|")[0]) for line in counter_lines)
  asserts.assertGreater(total, 0)
  asserts.assertLessEqual(total, int(counters["benchmark.http_2xx"]))

  latency_lines = [
      line for line in collector.lines
      if line.startswith("nighthawk.worker.0.benchmark_http_client.latency_2xx:")
  ]
  asserts.assertGreaterEqual(len(latency_lines), 100)
  for line in latency_lines[:10]:
    asserts.assertIn("|ms", line)
    millis = float(line.split(":")[1].split("|")[0])
    # Loopback latencies: sub-millisecond up to a few hundred milliseconds, never raw nanoseconds.
    asserts.assertBetweenInclusive(millis, 0.0, 1000.0)


def test_dog_statsd_sink_emits_worker_tags(http_test_server_fixture):
  """The DogStatsD variant strips the per-worker prefix and tags instead."""
  collector = UdpCollector(http_test_server_fixture.ip_version)
  try:
    http_test_server_fixture.runNighthawkClient([
        http_test_server_fixture.getTestServerRootUri(), "--rps", "50", "--duration", "2",
        "--stats-flush-interval", "1", "--stats-sinks",
        _sink_config("envoy.stat_sinks.dog_statsd", "DogStatsdSink", collector.address,
                     collector.port), "--stats-sink-tag", "run:it"
    ])
  finally:
    collector.stop()
  tagged = [line for line in collector.lines if line.startswith("nighthawk.benchmark.http_2xx:")]
  asserts.assertGreaterEqual(len(tagged), 1)
  for line in tagged:
    asserts.assertIn("|c|#worker:0,run:it", line)
  asserts.assertGreaterEqual(
      len([
          line for line in collector.lines
          if "benchmark_http_client.latency_2xx:" in line and "|ms|#worker:0,run:it" in line
      ]), 50)
