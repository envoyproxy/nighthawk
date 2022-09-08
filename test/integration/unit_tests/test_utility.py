"""Contains unit tests for functions in utility.py."""

import pytest
import re

from test.integration import utility


def test_get_execution_duration_from_global_result_json_retrieves_duration():
  """Test for the successful case."""
  global_result_json = {"execution_duration": "5s"}
  duration = utility.get_execution_duration_from_global_result_json(global_result_json)
  assert duration == 5


def test_get_execution_duration_from_global_result_json_retrieves_zero_duration():
  """Test when execution duration is zero."""
  global_result_json = {"execution_duration": "0s"}
  duration = utility.get_execution_duration_from_global_result_json(global_result_json)
  assert duration == 0


def test_get_execution_duration_from_global_result_json_retrieves_float_duration():
  """Test when execution duration is a float."""
  global_result_json = {"execution_duration": "2.7s"}
  duration = utility.get_execution_duration_from_global_result_json(global_result_json)
  assert duration == 2.7


def test_get_execution_duration_from_global_result_json_missing_duration():
  """Test for the failure when execution_duration is missing."""
  with pytest.raises(utility.Error):
    utility.get_execution_duration_from_global_result_json({})


def test_get_execution_duration_from_global_result_json_missing_suffix():
  """Test for the failure when execution_duration is missing the 's' suffix."""
  global_result_json = {"execution_duration": "5"}
  with pytest.raises(utility.Error):
    utility.get_execution_duration_from_global_result_json(global_result_json)


def test_count_log_lines_with_substring_counts_only_lines_with_substring():
  """Test that only lines with substring are counted."""
  logs = """
  DEBUG: log example 1 \r\n
  ERROR: log example 2 \r\n
  ERROR: log example 3 \r\n
  INFO: log example 1 \r\n
  """
  count = utility.count_log_lines_with_substring(logs, "ERROR")
  assert count == 2


def test_count_log_lines_with_substring_returns_zero_for_no_matches():
  """Test that returns zero if there are no matching lines."""
  logs = """
  DEBUG: log example 1 \r\n
  ERROR: log example 2 \r\n
  ERROR: log example 3 \r\n
  INFO: log example 1 \r\n
  """
  count = utility.count_log_lines_with_substring(logs, "log example 4")
  assert count == 0


def test_count_log_lines_with_substring_returns_zero_with_empty_logs():
  """Test that returns zero if there are no logs."""
  logs = ""
  count = utility.count_log_lines_with_substring(logs, "log example 4")
  assert count == 0


def test_parse_uris_to_socket_address():
  """Test parse uri for both ipv4 and ipv6."""
  addresses = ["http://1.2.3.45:2022", "http://2001:db8:3333:4444:CCCC:DDDD:EEEE:FFFF:9001"]
  v4_address, v6_address = utility.parseUrisToSocketAddress(addresses)
  assert v4_address.ip == "1.2.3.45" and v4_address.port == 2022
  assert v6_address.ip == "2001:db8:3333:4444:CCCC:DDDD:EEEE:FFFF" and v6_address.port == 9001


if __name__ == "__main__":
  raise SystemExit(pytest.main([__file__]))
