class NighthawkException(Exception):
  pass


# TODO(oschaaf): When we're on python 3 teach IpVersion below how to render itself to a string.
class IpVersion:
  UNKNOWN = 1
  IPV4 = 2
  IPV6 = 3
