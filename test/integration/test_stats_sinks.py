"""Tests for stats sinks implemented as Envoy extensions, used through the adapter sink."""

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


def _adapter_config(envoy_sink_name, envoy_sink_type, address, port):
  """Return a --stats-sinks value that forwards to the given Envoy statsd flavored sink."""
  return ("{name:\"nighthawk.envoy_stats_sink_adapter\",typed_config:{\"@type\":"
          "\"type.googleapis.com/nighthawk.EnvoyStatsSinkAdapterConfig\",sink:{name:\"%s\","
          "typed_config:{\"@type\":\"type.googleapis.com/envoy.config.metrics.v3.%s\","
          "address:{socket_address:{address:\"%s\",port_value:%d}},prefix:\"nighthawk\","
          "scale_histogram_units_to_milliseconds:true}}}}") % (envoy_sink_name, envoy_sink_type,
                                                               address, port)


def _timing_millis(line):
  """Return the value of a statsd timing line such as 'name:1.5|ms'."""
  return float(line.split(":")[1].split("|")[0])


def test_envoy_statsd_sink_receives_counters_and_latencies(http_test_server_fixture):
  """Run with Envoy's statsd sink and check counters and latency timings arrive over UDP."""
  collector = UdpCollector(http_test_server_fixture.ip_version)
  try:
    parsed_json, _ = http_test_server_fixture.runNighthawkClient([
        http_test_server_fixture.getTestServerRootUri(), "--rps", "50", "--duration", "3",
        "--stats-flush-interval", "1", "--stats-sinks",
        _adapter_config("envoy.stat_sinks.statsd", "StatsdSink", collector.address, collector.port)
    ])
  finally:
    collector.stop()
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterGreaterEqual(counters, "benchmark.http_2xx", 100)

  counter_lines = [
      line for line in collector.lines if line.startswith("nighthawk.cluster.0.benchmark.http_2xx:")
  ]
  asserts.assertGreaterEqual(len(counter_lines), 1)
  for line in counter_lines:
    asserts.assertIn("|c", line)
  # Counter deltas across flushes add up to (at most) the final counter value.
  total = sum(int(line.split(":")[1].split("|")[0]) for line in counter_lines)
  asserts.assertGreater(total, 0)
  asserts.assertLessEqual(total, int(counters["benchmark.http_2xx"]))

  # One timing per request, named after the worker like the worker's other statistics.
  latency_lines = [
      line for line in collector.lines
      if line.startswith("nighthawk.cluster.0.benchmark_http_client.latency_2xx:")
  ]
  asserts.assertGreaterEqual(len(latency_lines), 100)
  for line in latency_lines:
    asserts.assertIn("|ms", line)
    # Loopback latencies in milliseconds: never the raw nanoseconds Nighthawk records, which
    # would be six orders of magnitude larger.
    asserts.assertBetweenInclusive(_timing_millis(line), 0.0, 1000.0)
  asserts.assertGreater(max(_timing_millis(line) for line in latency_lines), 0.0)


def test_envoy_dog_statsd_sink_tags_latencies_with_the_worker(http_test_server_fixture):
  """Run with Envoy's DogStatsD sink and check the worker arrives as a tag."""
  collector = UdpCollector(http_test_server_fixture.ip_version)
  try:
    http_test_server_fixture.runNighthawkClient([
        http_test_server_fixture.getTestServerRootUri(), "--rps", "50", "--duration", "2",
        "--stats-flush-interval", "1", "--stats-sinks",
        _adapter_config("envoy.stat_sinks.dog_statsd", "DogStatsdSink", collector.address,
                        collector.port)
    ])
  finally:
    collector.stop()
  latency_lines = [
      line for line in collector.lines
      if line.startswith("nighthawk.cluster.benchmark_http_client.latency_2xx:")
  ]
  asserts.assertGreaterEqual(len(latency_lines), 50)
  for line in latency_lines:
    asserts.assertIn("|ms|#envoy.cluster_name:0", line)
    asserts.assertBetweenInclusive(_timing_millis(line), 0.0, 1000.0)
