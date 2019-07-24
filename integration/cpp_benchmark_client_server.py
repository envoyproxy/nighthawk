#!/usr/bin/env python3
"""@package integration_test.py
Entry point for our integration testing
"""

import logging
import os
import sys
import unittest

from common import IpVersion, NighthawkException
from integration_test_fixtures import (HttpIntegrationTestBase, HttpsIntegrationTestBase,
                                       IntegrationTestBase)

assert sys.version_info >= (3, 0)

httpbase = None


def serverStartHook(ip_version, is_https):
  IntegrationTestBase.ip_version = ip_version
  logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)
  global httpbase

  if is_https:
    httpbase = HttpsIntegrationTestBase()
    httpbase.overrideTestServerConfigPath("test/test_data/benchmark_https_client_test_envoy.yaml")
  else:
    httpbase = HttpIntegrationTestBase()
    httpbase.overrideTestServerConfigPath("test/test_data/benchmark_http_client_test_envoy.yaml")

  httpbase.setUp()
  return httpbase.server_port


def getRunningServerPid():
  return httpbase.getServerPid()


def waitForExit():
  return httpbase.waitForServerExit()


def main():
  if len(sys.argv) != 3:
    print("cpp_benchmark_client_server.py [ipv4|ipv6] [http|https]")
    return -1
  port = serverStartHook(
      IpVersion.IPV6 if str.lower(sys.argv[1]) == "ipv6" else IpVersion.IPV4,
      str.lower(sys.argv[2]) == "https")
  print(str(port))
  print(str(getRunningServerPid()))
  sys.stdout.flush()
  return waitForExit()


if __name__ == '__main__':
  main()
