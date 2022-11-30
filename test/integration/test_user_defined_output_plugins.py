"""Tests Nighthawk's user defined output framework."""

import json
import logging
import math
import os
import pytest
import subprocess
import sys
import time
from threading import Thread

from test.integration.common import IpVersion
from test.integration.integration_test_fixtures import (http_test_server_fixture, server_config)
from test.integration import asserts
from test.integration import utility


def getUserDefinedOutputsFromJson(parsed_json):
  """Get the user defined outputs indexed by the result they came from."""
  return {result["name"]: result["user_defined_outputs"] for result in parsed_json["results"]}


# logging_plugin = "{name:\"nighthawk.fake_user_defined_output\",typed_config:{\"@type\":\"type.googleapis.com/nighthawk.LogResponseHeadersConfig\"}}"


@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_all_plugin_apis_called(http_test_server_fixture):
  """Checks that a User Defined Output Plugin produces correct output."""
  fake_plugin_config = (
      "{name:\"nighthawk.fake_user_defined_output\","
      "typed_config:{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\"}}")

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100", "--concurrency", "2",
      "--termination-predicate", "benchmark.http_2xx:24", "--rps", "100",
      "--user-defined-plugin-config", fake_plugin_config
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterGreaterEqual(counters, "benchmark.http_2xx", 50)

  user_defined_outputs_by_result = getUserDefinedOutputsFromJson(parsed_json)
  asserts.assertEqual(len(user_defined_outputs_by_result), 3)

  for result_name in user_defined_outputs_by_result:
    user_defined_outputs = user_defined_outputs_by_result[result_name]
    asserts.assertEqual(len(user_defined_outputs), 1)
    user_defined_output = user_defined_outputs[0]
    asserts.assertEqual(user_defined_output["plugin_name"], "nighthawk.fake_user_defined_output")
    if result_name == "global":
      asserts.assertGreaterEqual(user_defined_output["typed_output"]["data_called"], 50)
      asserts.assertGreaterEqual(user_defined_output["typed_output"]["headers_called"], 50)
    else:
      asserts.assertGreaterEqual(user_defined_output["typed_output"]["data_called"], 25)
      asserts.assertGreaterEqual(user_defined_output["typed_output"]["headers_called"], 25)


@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_multiple_plugins_succeed(http_test_server_fixture):
  """Checks that a User Defined Output Plugin produces correct output."""
  fake_plugin_config = (
      "{name:\"nighthawk.fake_user_defined_output\","
      "typed_config:{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\"}}")
  logging_plugin_config = (
      "{name:\"nighthawk.log_response_headers_plugin\","
      "typed_config:{\"@type\":\"type.googleapis.com/nighthawk.LogResponseHeadersConfig\","
      "logging_mode:\"LM_SKIP_200_LEVEL_RESPONSES\"}}")

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100", "--concurrency", "1",
      "--termination-predicate", "benchmark.http_2xx:24", "--rps", "100",
      "--user-defined-plugin-config", fake_plugin_config, "--user-defined-plugin-config",
      logging_plugin_config
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterGreaterEqual(counters, "benchmark.http_2xx", 25)

  user_defined_outputs_by_result = getUserDefinedOutputsFromJson(parsed_json)
  asserts.assertEqual(len(user_defined_outputs_by_result), 1)

  global_user_defined_outputs = user_defined_outputs_by_result['global']
  asserts.assertEqual(len(global_user_defined_outputs), 2)
  for user_defined_output in global_user_defined_outputs:
    if user_defined_output["plugin_name"] == "nighthawk.fake_user_defined_output":
      asserts.assertGreaterEqual(user_defined_output["typed_output"]["data_called"], 25)
      asserts.assertGreaterEqual(user_defined_output["typed_output"]["headers_called"], 25)
    else:
      # There should not be any plugins that don't have the two above names.
      asserts.assertEqual(user_defined_output["plugin_name"],
                          "nighthawk.log_response_headers_plugin")


@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_handle_headers_failure_increments_counter(http_test_server_fixture):
  """Checks that a User Defined Output Plugin produces correct output."""
  fake_plugin_config = (
      "{name:\"nighthawk.fake_user_defined_output\","
      "typed_config:{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\","
      "fail_headers:true,header_failure_countdown:9}}")

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100", "--concurrency", "1",
      "--termination-predicate", "benchmark.user_defined_plugin_handle_headers_failure:10", "--rps",
      "100", "--user-defined-plugin-config", fake_plugin_config
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterGreaterEqual(counters, "benchmark.http_2xx", 20)

  user_defined_outputs_by_result = getUserDefinedOutputsFromJson(parsed_json)
  asserts.assertEqual(len(user_defined_outputs_by_result), 1)

  global_user_defined_outputs = user_defined_outputs_by_result["global"]
  asserts.assertEqual(len(global_user_defined_outputs), 1)
  user_defined_output = global_user_defined_outputs[0]
  asserts.assertEqual(user_defined_output["plugin_name"], "nighthawk.fake_user_defined_output")
  asserts.assertGreaterEqual(user_defined_output["typed_output"]["data_called"], 20)
  asserts.assertGreaterEqual(user_defined_output["typed_output"]["headers_called"], 20)

  expected_failure_counter = user_defined_output["typed_output"]["headers_called"] - 9
  asserts.assertCounterEqual(counters, "benchmark.user_defined_plugin_handle_headers_failure",
                             expected_failure_counter)


@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_handle_data_failure_increments_counter(http_test_server_fixture):
  """Checks that a User Defined Output Plugin produces correct output."""
  fake_plugin_config = (
      "{name:\"nighthawk.fake_user_defined_output\","
      "typed_config:{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\","
      "fail_data:true,data_failure_countdown:9}}")

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100", "--concurrency", "1",
      "--termination-predicate", "benchmark.user_defined_plugin_handle_data_failure:10", "--rps",
      "100", "--user-defined-plugin-config", fake_plugin_config
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterGreaterEqual(counters, "benchmark.http_2xx", 20)

  user_defined_outputs_by_result = getUserDefinedOutputsFromJson(parsed_json)
  asserts.assertEqual(len(user_defined_outputs_by_result), 1)

  global_user_defined_outputs = user_defined_outputs_by_result["global"]
  asserts.assertEqual(len(global_user_defined_outputs), 1)
  user_defined_output = global_user_defined_outputs[0]
  asserts.assertEqual(user_defined_output["plugin_name"], "nighthawk.fake_user_defined_output")
  asserts.assertGreaterEqual(user_defined_output["typed_output"]["data_called"], 20)
  asserts.assertGreaterEqual(user_defined_output["typed_output"]["headers_called"], 20)

  expected_failure_counter = user_defined_output["typed_output"]["data_called"] - 9
  asserts.assertCounterEqual(counters, "benchmark.user_defined_plugin_handle_data_failure",
                             expected_failure_counter)


@pytest.mark.parametrize('server_config',
                         ["nighthawk/test/integration/configurations/nighthawk_http_origin.yaml"])
def test_output_generation_produces_errors_successfully(http_test_server_fixture):
  """Checks that a User Defined Output Plugin produces correct output."""
  fake_plugin_config = (
      "{name:\"nighthawk.fake_user_defined_output\","
      "typed_config:{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\","
      "fail_per_worker_output:true}}")

  parsed_json, _ = http_test_server_fixture.runNighthawkClient([
      http_test_server_fixture.getTestServerRootUri(), "--duration", "100", "--concurrency", "2",
      "--termination-predicate", "benchmark.http_2xx:24", "--rps", "100",
      "--user-defined-plugin-config", fake_plugin_config
  ])
  counters = http_test_server_fixture.getNighthawkCounterMapFromJson(parsed_json)
  asserts.assertCounterGreaterEqual(counters, "benchmark.http_2xx", 25)

  user_defined_outputs_by_result = getUserDefinedOutputsFromJson(parsed_json)
  asserts.assertEqual(len(user_defined_outputs_by_result), 3)

  for result_name in user_defined_outputs_by_result:
    user_defined_outputs = user_defined_outputs_by_result[result_name]
    asserts.assertEqual(len(user_defined_outputs), 1)
    user_defined_output = user_defined_outputs[0]
    if result_name == "global":
      asserts.assertIn("Cannot aggregate if any per_worker_outputs failed", user_defined_output["error_message"])
    else:
      asserts.assertIn("Intentional FakeUserDefinedOutputPlugin failure on getting PerWorkerOutput", user_defined_output["error_message"])


# test get_output failure
# test aggregation failure
