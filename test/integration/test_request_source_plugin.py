"""Tests for the nighthawk_service binary."""

import pytest

from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import utility
from test.integration import asserts


def test_request_source_plugin_happy_flow(http_test_server_fixture):
  """Test that the nighthawkClient can run with request-source-plugin option."""
  request_source_config ="""
  {
  name:"nighthawk.in-line-options-list-request-source-plugin",
  typed_config:{
  "@type":"type.googleapis.com/nighthawk.request_source.InLineOptionsListRequestSourceConfig",
  options_list:{
  options:[{request_method:"1",request_headers:[{header:{"key":"key","value":"value"}}]}]
  },
  }
  }"""
  # request_source_config =(
  # "{"
  # "name:\"nighthawk.in-line-options-list-request-source-plugin\","
  # "typed_config:{"
  # "\"@type\":\"type.googleapis.com/"
  # "nighthawk.request_source.InLineOptionsListRequestSourceConfig\","
  # "options_list:{"
  # "options:[{request_method:\"1\",request_headers:[{header:{key:\"key\",value:\"value\"}"
  # "}]}]"
  # "},"
  # "}")

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--termination-predicate", "benchmark.http_2xx:5", "--rps 10",
      "--request-source-plugin-config %s" % request_source_config,
      http_test_server_fixture.getTestServerRootUri()
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertGreaterEqual(counters["benchmark.http_2xx"], 5)
  asserts.assertEqual(counters["requestsource.internal.upstream_rq_200"], 1)