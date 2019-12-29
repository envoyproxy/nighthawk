import os


def assertEqual(a, b):
  assert a == b


def assertGreater(a, b):
  assert a > b


def assertGreaterEqual(a, b):
  assert a >= b


def assertLessEqual(a, b):
  assert a <= b


def assertNotIn(a, b):
  assert a not in b


def assertIn(a, b):
  assert a in b


def assertBetweenInclusive(a, min_value, max_value):
  assertGreaterEqual(a, min_value)
  assertLessEqual(a, max_value)


def assertCounterEqual(counters, name, value):
  assertIn(name, counters)
  assertEqual(counters[name], value)


def assertCounterGreater(counters, name, value):
  assertIn(name, counters)
  assertGreater(counters[name], value)


def assertCounterGreaterEqual(counters, name, value):
  assertIn(name, counters)
  assertGreaterEqual(counters[name], value)


def assertCounterLessEqual(counters, name, value):
  assertIn(name, counters)
  assertLessEqual(counters[name], value)


def assertCounterBetweenInclusive(counters, name, min_value, max_value):
  assertIn(name, counters)
  assertBetweenInclusive(counters[name], min_value, max_value)


def isSanitizerRun():
  return True if os.environ.get("NH_INTEGRATION_TEST_SANITIZER_RUN", 0) == "1" else False


def isCoverageRun():
  return True if os.environ.get("ENVOY_CONFIG_COVERAGE", 0) == "1" else False
