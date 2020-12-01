"""Contains unit tests for functions in nighthawk_test_server.py."""

import pytest

from test.integration import nighthawk_test_server


def test_extractWarningsAndErrors_nothing_on_empty_output():
  """Test with an empty input."""
  warnings, errors = nighthawk_test_server._extractWarningsAndErrors("", [])
  assert not warnings
  assert not errors


def test_extractWarningsAndErrors_ignores_info_logs():
  """Test where the process output doesn't contain any warnings or errors."""
  process_output = """
  [2020-12-01 04:41:57.219][126][info][misc] Message.
  """
  warnings, errors = nighthawk_test_server._extractWarningsAndErrors(process_output, [])
  assert not warnings
  assert not errors


def test_extractWarningsAndErrors_extracts_a_warning():
  """Test where the process output contains a single warning."""
  process_output = "[2020-12-01 04:41:57.219][126][warning][misc] Message."
  warnings, errors = nighthawk_test_server._extractWarningsAndErrors(process_output, [])
  assert warnings == ["[2020-12-01 04:41:57.219][126][warning][misc] Message."]
  assert not errors


def test_extractWarningsAndErrors_extracts_an_error():
  """Test where the process output contains a single error."""
  process_output = "[2020-12-01 04:41:57.219][126][error][misc] Message."
  warnings, errors = nighthawk_test_server._extractWarningsAndErrors(process_output, [])
  assert not warnings
  assert errors == ["[2020-12-01 04:41:57.219][126][error][misc] Message."]


def test_extractWarningsAndErrors_extracts_multiple_messages():
  """Test where the process output contains multiple warnings and errors."""
  process_output = """[warning][misc] Warning1.
[error][misc] Error1.
[info][misc] Info1.
[error][runtime] Error2.
[warning][runtime] Warning2.
  """
  warnings, errors = nighthawk_test_server._extractWarningsAndErrors(process_output, [])
  assert warnings == ["[warning][misc] Warning1.", "[warning][runtime] Warning2."]
  assert errors == ["[error][misc] Error1.", "[error][runtime] Error2."]


def test_extractWarningsAndErrors_skips_messages_matching_ignore_list():
  """Test where the ignore list is used."""
  process_output = """[warning][misc] Warning1 foo.
[error][misc] Error1 bar.
[info][misc] Info1.
[error][runtime] Error2 baz.
[warning][runtime] Warning2 bar.
  """

  ignore_list = ["foo", "bar"]
  warnings, errors = nighthawk_test_server._extractWarningsAndErrors(process_output, ignore_list)
  assert not warnings
  assert errors == ["[error][runtime] Error2 baz."]


if __name__ == "__main__":
  raise SystemExit(pytest.main([__file__]))
