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
