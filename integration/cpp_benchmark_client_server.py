#!/usr/bin/env python3
"""@package integration_test.py
Entry point for our integration testing
"""

import logging
import os
import sys
import unittest
import cpp_benchmark_client_server

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


def serverStartHookHttpIPV4():
  return serverStartHook(IpVersion.IPV4, False)


def serverStartHookHttpIPV6():
  return serverStartHook(IpVersion.IPV6, False)


def serverStartHookHttpsIPV4():
  return serverStartHook(IpVersion.IPV4, True)


def serverStartHookHttpsIPV6():
  return serverStartHook(IpVersion.IPV6, True)


def getRunningServerPid():
  return httpbase.getServerPid()


def waitForExit():
  return httpbase.waitForServerExit()
