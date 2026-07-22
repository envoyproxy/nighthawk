"""Tests for Rate Limiter Plugins."""

import pytest
from test.integration.integration_test_fixtures import http_test_server_fixture
from test.integration import asserts


@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_linear_ramping_rate_limiter_plugin(http_test_server_fixture):
  """Test LinearRampingRateLimiter plugin.

  Ramp time: 5.5 seconds
  Duration: 12 seconds
  RPS: 100
  Expected total requests: 925 (margin of 5)
  """
  rate_limiter_config = (
      "{name:\"nighthawk.linear-ramping-rate-limiter-plugin\","
      "typed_config:{\"@type\":\"type.googleapis.com/nighthawk.rate_limiter.LinearRampingRateLimiterConfig\","
      "ramp_time:{seconds:5,nanos:500000000}}}")  # 5.5 seconds

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "12", "--concurrency", "1",
      "--rps", "100", "--rate-limiter-plugin-config", rate_limiter_config
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)

  expected_total = 925
  margin = 5
  asserts.assertCounterBetweenInclusive(
      counters, "benchmark.http_2xx", expected_total - margin, expected_total + margin
  )
