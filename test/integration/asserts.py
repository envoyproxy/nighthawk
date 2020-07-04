"""A library with assertation helpers for unit tests."""


def assertEqual(a, b):
  """Assert that arguments a and b are equal."""
  assert a == b


def assertGreater(a, b):
  """Assert that a is greather than b."""
  assert a > b


def assertGreaterEqual(a, b):
  """Assert that a is greather than or equal to b."""
  assert a >= b


def assertLessEqual(a, b):
  """Assert that a is less than or equal to b."""
  assert a <= b


def assertNotIn(a, b):
  """Assert that a is not contained in b."""
  assert a not in b


def assertIn(a, b):
  """Assert that a is contained in b."""
  assert a in b


def assertBetweenInclusive(a, min_value, max_value):
  """Assert that the passed value is between min_value and max_value, inclusive."""
  assertGreaterEqual(a, min_value)
  assertLessEqual(a, max_value)


def assertCounterEqual(counters, name, value):
  """Assert that a counter with the specified name is present with the specified value.

  Args:
      counters (map): Counter values keyed by name.
      name (string): Name of the counter under test.
      value ([int]): Value that the counter value will be compared against.
  """
  assertIn(name, counters)
  assertEqual(counters[name], value)


def assertCounterGreater(counters, name, value):
  """Assert that a counter with the specified name is present and its value is greater than the specified value.

  Args:
      counters (map): Counter values keyed by name.
      name (string): Name of the counter under test.
      value ([int]): Value that the counter value will be compared against.
  """
  assertIn(name, counters)
  assertGreater(counters[name], value)


def assertCounterGreaterEqual(counters, name, value):
  """Assert that a counter with the specified name is present and its value is greater than or equal to the specified value.

  Args:
      counters (map): Counter values keyed by name.
      name (string): Name of the counter under test.
      value ([int]): Value that the counter value will be compared against.
  """
  assertIn(name, counters)
  assertGreaterEqual(counters[name], value)


def assertCounterLessEqual(counters, name, value):
  """Assert that a counter with the specified name is present and its value is less than or equal to the specified value.

  Args:
      counters (map): Counter values keyed by name.
      name (string): Name of the counter under test.
      value ([int]): Value that the counter value will be compared against.
  """
  assertIn(name, counters)
  assertLessEqual(counters[name], value)


def assertCounterBetweenInclusive(counters, name, min_value, max_value):
  """Assert that a counter with the specified name is present and its value is between the specified minimum and maximum, inclusive.

  Args:
      counters (map): Counter values keyed by name.
      name (string): Name of the counter under test.
      min_value ([int]): Minimum value that the counter may have.
      max_value ([int]): Maximum value that the counter may have.
  """
  assertIn(name, counters)
  assertBetweenInclusive(counters[name], min_value, max_value)
