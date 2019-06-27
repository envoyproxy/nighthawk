"""@package integration_test.py
Entry point for our integration testing
"""

import logging
import os
import sys
import unittest

import test_integration_basics
from common import IpVersion, NighthawkException
from integration_test_fixtures import IntegrationTestBase


def determineIpVersionsFromEnvironment():
  env_versions = os.environ.get("ENVOY_IP_TEST_VERSIONS", "all")
  if env_versions == "v4only":
    versions = [IpVersion.IPV4]
  elif env_versions == "v6only":
    versions = [IpVersion.IPV6]
  elif env_versions == "all":
    versions = [IpVersion.IPV4, IpVersion.IPV6]
  else:
    raise NighthawkException("Unknown ip version: '%s'" % versions)
  return versions


def runTests(ip_version):
  IntegrationTestBase.ip_version = ip_version
  logging.info("Running %s integration tests." %
               ("ipv6" if IntegrationTestBase.ip_version == IpVersion.IPV6 else "ipv4"))
  res = unittest.TextTestRunner(verbosity=2).run(
      unittest.TestLoader().loadTestsFromModule(test_integration_basics))
  return res.wasSuccessful()


if __name__ == '__main__':
  logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)
  ip_versions = determineIpVersionsFromEnvironment()
  assert (len(ip_versions) > 0)
  ok = all(map(runTests, ip_versions))
  exit(0 if ok else 1)
