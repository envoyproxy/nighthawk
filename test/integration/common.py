"""Miscellaneous utilities used in the integration tests."""

from enum import Enum


class NighthawkException(Exception):
  """Base exception class for Nighthawk's python code."""


# TODO(oschaaf): When we're on python 3 teach IpVersion below how to render itself to a string.
class IpVersion(Enum):
  """Enumerate IP versions."""

  UNKNOWN = 1
  IPV4 = 2
  IPV6 = 3
