"""Tests for the nighthawk_service binary."""

import pytest
import os

from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import utility
from test.integration import asserts


@pytest.mark.parametrize(
    "request_source_config,expected_min,expected_max",
    [
        pytest.param("""
  {
  name:"nighthawk.in-line-options-list-request-source-plugin",
  typed_config:{
  "@type":"type.googleapis.com/nighthawk.request_source.InLineOptionsListRequestSourceConfig",
  options_list:{
  options:[
    {request_method:"1",request_body_size:"1",request_headers:[{header:{"key":"x-nighthawk-test-server-config","value":"{response_body_size:13}"}}]},
    {request_method:"1",request_body_size:"2",request_headers:[{header:{"key":"x-nighthawk-test-server-config","value":"{response_body_size:17}"}}]},
    ]
  },
  }
  }""",
                     13,
                     17,
                     id="in-line"),
        pytest.param("""
  {
  name:"nighthawk.file-based-request-source-plugin",
  typed_config:{
  "@type":"type.googleapis.com/nighthawk.request_source.FileBasedOptionsListRequestSourceConfig",
  file_path:"%s",
  }
  }""" % (os.path.dirname(os.path.abspath(os.path.dirname(__file__))) +
          "/request_source/test_data/test-config.yaml"),
                     13,
                     17,
                     id="file-based"),
    ],
)
def test_request_source_plugin_happy_flow_parametrized(http_test_server_fixture,
                                                       request_source_config, expected_min,
                                                       expected_max):
  """Test that the nighthawkClient can run with request-source-plugin option."""
  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      "--termination-predicate", "benchmark.http_2xx:5", "--rps 10",
      "--request-source-plugin-config %s" % request_source_config,
      http_test_server_fixture.getTestServerRootUri(), "--request-header", "host: sni.com"
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  global_histograms = http_test_server_fixture.getNighthawkGlobalHistogramsbyIdFromJson(parsed_json)
  asserts.assertGreaterEqual(counters["benchmark.http_2xx"], 5)
  asserts.assertEqual(int(global_histograms["benchmark_http_client.response_body_size"]["raw_max"]),
                      expected_max)
  asserts.assertEqual(int(global_histograms["benchmark_http_client.response_body_size"]["raw_min"]),
                      expected_min)
