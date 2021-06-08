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


if __name__ == "__main__":
  raise SystemExit(pytest.main([__file__]))
