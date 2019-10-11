#!/usr/bin/env python3

import logging
import os
import sys
import pytest

from test.integration.common import IpVersion
from test.integration.integration_test_fixtures import (http_test_server_fixture,
                                                        https_test_server_fixture)
from test.integration.utility import *
from test.integration.test_integration_basics import mini_stress_test_h1


def test_http_h1_mini_stress_test_without_client_side_queueing(http_test_server_fixture):
  http_test_server_fixture.test_server.enableCpuProfiler()
  counters = mini_stress_test_h1(
      http_test_server_fixture,
      [http_test_server_fixture.getTestServerRootUri(), "--rps", "999999", "--duration 2"])
  print(str(counters))
