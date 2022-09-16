"""Tests for dynamic_config_manager."""

from nighthawk.dynamic_config import dynamic_config_manager
from nighthawk.api.configuration import cluster_config_manager_pb2, endpoints_config_manager_pb2
from envoy.config.endpoint.v3 import endpoint_pb2
from unittest import mock
import pytest
import random

from envoy.service.discovery.v3 import discovery_pb2


def toggle_bool(choices):
  """Use to deterministically patch random choice when initially assigning resources, starting with True."""
  if not hasattr(toggle_bool, "next_value"):
    toggle_bool.next_value = True
  ret = toggle_bool.next_value
  toggle_bool.next_value = not ret
  return ret


def _add_endpoint(cluster, ip, port):
  endpoint = cluster.endpoints.add()
  endpoint.ip = ip
  endpoint.port = port


@pytest.fixture()
def cluster_config():
  """Yield the config."""
  config = cluster_config_manager_pb2.DynamicClusterConfigManagerSettings()
  cluster_one = config.clusters.add()
  cluster_one.name = "foo"
  _add_endpoint(cluster_one, '127.0.0.1', 1234)
  _add_endpoint(cluster_one, '127.0.0.1', 4567)

  cluster_two = config.clusters.add()
  cluster_two.name = "bar"
  _add_endpoint(cluster_two, '192.168.0.1', 1234)
  _add_endpoint(cluster_two, '192.168.0.1', 4567)
  yield config


def test_initialize_clusters_randomly(cluster_config):
  """Test that clusters are randomly selected in initial configuration."""
  with mock.patch('random.choice', side_effect=toggle_bool) as _:
    cluster_manager = dynamic_config_manager.DynamicClusterConfigManager(cluster_config)
    config = discovery_pb2.DiscoveryResponse()
    config.ParseFromString(cluster_manager.serialize())
    assert (len(config.resources) == 1)


def test_can_mutate_clusters(cluster_config):
  """Test that we can randomly mutate the configuration."""
  with mock.patch('random.choice', side_effect=toggle_bool) as _:
    cluster_manager = dynamic_config_manager.DynamicClusterConfigManager(cluster_config)

  cluster_manager.mutate()
  config = discovery_pb2.DiscoveryResponse()
  config.ParseFromString(cluster_manager.serialize())

  # Due to randomness in mutate, we check valid mutation states after invoking.
  if cluster_manager.getLastMutateActionForTesting(
  ) == dynamic_config_manager.DynamicConfigManager.Action.ADD:
    assert (len(config.resources) == 1 or len(config.resources) == 2)
  elif cluster_manager.getLastMutateActionForTesting(
  ) == dynamic_config_manager.DynamicConfigManager.Action.REMOVE:
    assert (len(config.resources) == 1 or len(config.resources) == 0)
  else:
    raise NotImplementedError('Action: {} is not implemented.'.format(
        cluster_manager.getLastMutateActionForTesting()))


@pytest.fixture()
def endpoints_config():
  """Yield the config."""
  config = endpoints_config_manager_pb2.DynamicEndpointsConfigManagerSettings()
  config.cluster.name = "foo"
  _add_endpoint(config.cluster, '127.0.0.1', 1234)
  _add_endpoint(config.cluster, '127.0.0.1', 4567)
  _add_endpoint(config.cluster, '192.168.0.1', 1234)
  _add_endpoint(config.cluster, '192.168.0.1', 4567)
  yield config


def test_initialize_endpoints_randomly(endpoints_config):
  """Test that clusters are randomly selected in initial configuration."""
  with mock.patch('random.choice', side_effect=toggle_bool) as _:
    endpoints_manager = dynamic_config_manager.DynamicEndpointsConfigManager(endpoints_config)

  config = discovery_pb2.DiscoveryResponse()
  config.ParseFromString(endpoints_manager.serialize())
  assert (len(config.resources) == 1)

  initial_endpoints = endpoint_pb2.ClusterLoadAssignment()
  assert config.resources[0].Unpack(initial_endpoints)
  assert len(initial_endpoints.endpoints[0].lb_endpoints) == 2


def test_can_mutate_endpoints(endpoints_config):
  """Test that we can randomly mutate the endpoint configuration."""
  endpoints_manager = dynamic_config_manager.DynamicEndpointsConfigManager(endpoints_config)

  for i in range(100):
    endpoints_manager.mutate()
  config = discovery_pb2.DiscoveryResponse()
  config.ParseFromString(endpoints_manager.serialize())
  endpoints_after_mutation = endpoint_pb2.ClusterLoadAssignment()
  assert config.resources[0].Unpack(endpoints_after_mutation)

  num_endpoints_active = len(endpoints_after_mutation.endpoints[0].lb_endpoints)
  assert num_endpoints_active <= len(endpoints_config.cluster.endpoints)


if __name__ == '__main__':
  raise SystemExit(pytest.main(['-s', '-v', __file__]))
