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
from integration_test import determineIpVersionsFromEnvironment
from integration_test_fixtures import (HttpIntegrationTestBase, HttpsIntegrationTestBase,
                                       IntegrationTestBase)

assert sys.version_info >= (3, 0)

def runTests(ip_version):
  IntegrationTestBase.ip_version = ip_version
  logging.info("Running %s integration tests." %
               ("ipv6" if IntegrationTestBase.ip_version == IpVersion.IPV6 else "ipv4"))
  res = unittest.TextTestRunner(verbosity=2).run(
      unittest.TestLoader().loadTestsFromModule(cpp_benchmark_client_server))
  return True

httpbase = None

def serverStartHook():
  logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)
  ip_versions = determineIpVersionsFromEnvironment()
  #ok = all(map(runTests, ip_versions))
  global httpbase
  httpbase = TestHttp()
  httpbase.setUp()
  print("@@@@@@@@@@@@ " + str(httpbase.admin_port), file=sys.stderr)
  #httpbase.test_run_h1_server_for_cpp_benchmark_client_test()
  return httpbase.server_port

def getRunningServerPid():
  return httpbase.getServerPid()

class TestHttp(HttpIntegrationTestBase):

  def test_run_h1_server_for_cpp_benchmark_client_test(self):
    """
    Runs the CLI configured to use plain HTTP/1 against our test server, and sanity
    checks statistics from both client and server.
    """
    #globals["server_pid"] = self.getServerPid()
    #globals["server_port"] = self.server_port
    #globals["admin_port"] = self.admin_port
    #print(self.server_port, file=sys.stderr)
    #print(self.admin_port, file=sys.stderr)
    #sys.stderr.flush()
    #self.waitForExit()
