"""Tests for dynamic_config_manager."""

from dynamic_config_manager import DynamicClusterConfigManager
import nighthawk.api.configuration.cluster_config_manager_pb2 as cluster_manager_pb2
from unittest.mock import patch
import pytest
import random

import envoy.service.discovery.v3.discovery_pb2 as discovery_pb2


def toggle_add_cluster(choices):
  """Toggle random result deterministically starting with True."""
  if not hasattr(toggle_add_cluster, "add_cluster"):
    toggle_add_cluster.add_cluster = True
  ret = toggle_add_cluster.add_cluster
  toggle_add_cluster.add_cluster = not ret
  return ret


def _add_endpoint(cluster, ip, port):
  endpoint = cluster.endpoints.add()
  endpoint.ip = ip
  endpoint.port = port


@pytest.fixture()
def config():
  """Yield the config."""
  config = cluster_manager_pb2.DynamicClusterConfigManagerSettings()
  cluster_one = config.clusters.add()
  cluster_one.name = "foo"
  _add_endpoint(cluster_one, '127.0.0.1', 1234)
  _add_endpoint(cluster_one, '127.0.0.1', 4567)

  cluster_two = config.clusters.add()
  cluster_two.name = "bar"
  _add_endpoint(cluster_two, '192.168.0.1', 1234)
  _add_endpoint(cluster_two, '192.168.0.1', 4567)
  yield config


def test_initialize_clusters_randomly(config):
  """Test that clusters are randomly selected in initial configuration."""
  with patch('random.choice', side_effect=toggle_add_cluster) as _:
    cm = DynamicClusterConfigManager(config)
    config = discovery_pb2.DiscoveryResponse()
    config.ParseFromString(cm.serialize())
    assert (len(config.resources) == 1)


def test_can_mutate_clusters(config):
  """Test that we can randomly mutate the configuration."""
  with patch('random.choice', side_effect=toggle_add_cluster) as _:
    cm = DynamicClusterConfigManager(config)

  cm.mutate()
  config = discovery_pb2.DiscoveryResponse()
  config.ParseFromString(cm.serialize())

  # Due to randomness in mutate, we check valid mutation states after invoking.
  if cm.getLastMutateActionForTesting() == DynamicClusterConfigManager.Action.ADD:
    assert (len(config.resources) == 1 or len(config.resources) == 2)
  elif cm.getLastMutateActionForTesting() == DynamicClusterConfigManager.Action.REMOVE:
    assert (len(config.resources) == 1 or len(config.resources) == 0)
  else:
    raise NotImplementedError('Action: {} is not implemented.'.format(
        cm.getLastMutateActionForTesting()))


if __name__ == '__main__':
  raise SystemExit(pytest.main(['-s', '-v', __file__]))
