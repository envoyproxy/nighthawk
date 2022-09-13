"""Component that handles dynamic configuration of the system under test Envoy proxy."""

import json
import yaml
import enum
from google.protobuf import json_format, any_pb2

import abc
from envoy.config.cluster.v3 import cluster_pb2
from envoy.config.endpoint.v3 import endpoint_pb2, endpoint_components_pb2
from envoy.service.discovery.v3 import discovery_pb2
from envoy.extensions.upstreams.http.v3 import http_protocol_options_pb2
from envoy.extensions.transport_sockets.tls.v3 import tls_pb2
from nighthawk.api.configuration import cluster_config_manager_pb2, endpoints_config_manager_pb2

import random
import argparse
import time
import base64
import logging
import os


class DynamicConfigManager(abc.ABC):
  """Base class for Dynamic configuration components.

  Provides an interface for starting dynamic configuration.
  Users will invoke start() which periodically updates configuration
  until stop() is called.
  """

  class Action(enum.Enum):
    """Enum class for mutation actions to configuration."""

    ADD = 1
    REMOVE = 2

  mutate_actions = list(Action)

  def __init__(self):
    """Create a DynamicConfigManager."""
    self._exit = False

  def start(self):
    """Start periodically executing a method."""
    while not self._exit:
      time.sleep(self.timeBeforeNextUpdate())
      self.execute()

  def stop(self):
    """Stop periodically executing a method."""
    self._exit = True

  @abc.abstractmethod
  def timeBeforeNextUpdate(self):
    """Provide the time before the next execution."""
    pass

  @abc.abstractmethod
  def execute(self):
    """Periodically execute this method."""
    pass


class DynamicClusterConfigManager(DynamicConfigManager):
  """Encapsulates dynamic cluster configuration."""

  cluster_template_string = b"""
    name: some_service
    # Upstream TLS configuration.
    transport_socket:
      name: envoy.transport_sockets.tls
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext
    load_assignment:
      cluster_name: some_service
      # Static endpoint assignment.
      endpoints:
      - lb_endpoints:
    typed_extension_protocol_options:
      envoy.extensions.upstreams.http.v3.HttpProtocolOptions:
        "@type": type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions
        explicit_http_config:
          http2_protocol_options:
            max_concurrent_streams: 100
  """
  cluster_template = json_format.Parse(
      json.dumps(yaml.load(cluster_template_string, Loader=yaml.FullLoader)), cluster_pb2.Cluster())

  def __init__(self, config: cluster_config_manager_pb2.DynamicClusterConfigManagerSettings):
    """Create a DynamicClusterConfigManager."""
    DynamicConfigManager.__init__(self)
    self._active_config = discovery_pb2.DiscoveryResponse()
    self._active_config.version_info = "1"
    self._inactive_clusters = []
    clusters = self._parseAvailableClusters(config)
    self._randomlyAssignInitialClusters(clusters)
    # TODO(kbaichoo): refactor "new_cds.pb" into a shared constants file.
    self._output_file = config.output_file if config.output_file else "new_cds.pb"
    # TODO(kbaichoo): Handle nanos get ignored
    self._refresh_interval = config.refresh_interval.ToSeconds()
    # Tracks the last mutated action. Used for testing.
    self._last_mutate_action = None

  def _parseAvailableClusters(
      self, config: cluster_config_manager_pb2.DynamicClusterConfigManagerSettings):
    clusters = []
    for service_config in config.clusters:
      new_cluster = cluster_pb2.Cluster()
      new_cluster.CopyFrom(self.cluster_template)
      new_cluster.load_assignment.cluster_name = new_cluster.name = service_config.name
      for endpoint_config in service_config.endpoints:
        host = new_cluster.load_assignment.endpoints[0].lb_endpoints.add()
        socket_address = host.endpoint.address.socket_address
        socket_address.address = endpoint_config.ip
        socket_address.port_value = int(endpoint_config.port)
      clusters.append(new_cluster)
    return clusters

  def _randomlyAssignInitialClusters(self, clusters: list[cluster_pb2.Cluster]):
    while clusters:
      add_as_active_cluster = random.choice([True, False])
      if add_as_active_cluster:
        self._active_config.resources.add().Pack(clusters.pop())
      else:
        self._inactive_clusters.append(clusters.pop())

  def _addRandomClusters(self):
    # Randomly pick a number of clusters to add
    num_clusters_to_add = random.randrange(0, len(self._inactive_clusters) + 1)

    # All clusters active or 0 clusters chosen, do nothing.
    if not self._inactive_clusters or not num_clusters_to_add:
      return

    random.shuffle(self._inactive_clusters)

    for i in range(num_clusters_to_add):
      self._active_config.resources.add().Pack(self._inactive_clusters.pop())

  def _removeRandomClusters(self):
    resources = self._active_config.resources
    num_clusters_to_remove = random.randrange(0, len(resources) + 1)

    # All clusters inactive or 0 clusters chosen, do nothing.
    if not resources or not num_clusters_to_remove:
      return

    for i in range(num_clusters_to_remove):
      any_resource = resources.pop(random.randrange(0, len(resources)))
      unpacked_cluster = cluster_pb2.Cluster()
      any_resource.Unpack(unpacked_cluster)
      self._inactive_clusters.append(unpacked_cluster)

  def mutate(self):
    """Invoke to randomly mutate the current cluster configuration."""
    action = random.choice(self.mutate_actions)
    self._last_mutate_action = action
    if action == self.__class__.Action.ADD:
      self._addRandomClusters()
    elif action == self.__class__.Action.REMOVE:
      self._removeRandomClusters()
    else:
      raise NotImplementedError(f'Action: {action} is not implemented.')

  def getLastMutateActionForTesting(self):
    """Provide the last mutation action that occured for testing."""
    return self._last_mutate_action

  def timeBeforeNextUpdate(self):
    """Provide the time before the next execution."""
    return self._refresh_interval

  def execute(self):
    """Periodically execute this method."""
    self.mutate()
    self._active_config.version_info = str(int(self._active_config.version_info) + 1)
    self.serializeToFile()

  def serializeToFile(self):
    """Serialize current configuration of clusters to output file."""
    contents = self._active_config.SerializeToString()
    versioned_file = self._output_file + '.tmp'
    with open(versioned_file, 'wb') as f:
      f.write(contents)
    # Triggers the update to be picked up.
    os.replace(versioned_file, self._output_file)
    logging.info(
        f'Refreshed configuration at {self._output_file} new contents:\n{self._active_config}')

  def serialize(self) -> str:
    """Serialize current configuration of clusters."""
    return self._active_config.SerializeToString()


class DynamicEndpointsConfigManager(DynamicConfigManager):
  """Encapsulates dynamic endpoint configuration."""

  def __init__(self, config: endpoints_config_manager_pb2.DynamicEndpointsConfigManagerSettings):
    """Create a DynamicEndpointsConfigManager."""
    DynamicConfigManager.__init__(self)
    self._active_config = discovery_pb2.DiscoveryResponse()
    self._active_config.version_info = "1"
    self._active_config.resources.add()

    # The endpoints to publish
    self._cluster_load_assignment = endpoint_pb2.ClusterLoadAssignment()
    self._cluster_load_assignment.cluster_name = config.cluster.name
    self._cluster_load_assignment.endpoints.add()

    # Convience hook to add endpoints.
    self._lb_endpoints = self._cluster_load_assignment.endpoints[0].lb_endpoints

    endpoints = self._parseAvailableEndpoints(config.cluster.endpoints)
    self._inactive_endpoints = []
    self._randomlyAssignInitialEndpoints(endpoints)
    # TODO(kbaichoo): refactor into a shared constants file.
    self._output_file = config.output_file if config.output_file else "new_eds.pb"
    # TODO(kbaichoo): Handle nanos get ignored
    self._refresh_interval = config.refresh_interval.ToSeconds()
    # Tracks the last mutated action. Used for testing.
    self._last_mutate_action = None

  def _parseAvailableEndpoints(
      self,
      endpoints: list[endpoints_config_manager_pb2.DynamicEndpointsConfigManagerSettings.Endpoint]
  ) -> list[endpoint_components_pb2.LbEndpoint]:
    """Parse the available endpoints."""
    parsed_endpoints = []
    for endpoint_config in endpoints:
      host = endpoint_components_pb2.LbEndpoint()
      socket_address = host.endpoint.address.socket_address
      socket_address.address = endpoint_config.ip
      socket_address.port_value = int(endpoint_config.port)
      parsed_endpoints.append(host)
    return parsed_endpoints

  def _randomlyAssignInitialEndpoints(self, endpoints: list[endpoint_components_pb2.LbEndpoint]):
    while endpoints:
      add_as_active_endpoint = random.choice([True, False])
      if add_as_active_endpoint:
        self._lb_endpoints.append(endpoints.pop())
      else:
        self._inactive_endpoints.append(endpoints.pop())

  def mutate(self):
    """Invoke to randomly mutate the current endpoint configuration."""
    action = random.choice(self.mutate_actions)
    self._last_mutate_action = action
    if action == self.__class__.Action.ADD:
      self._addRandomEndpoints()
    elif action == self.__class__.Action.REMOVE:
      self._removeRandomEndpoints()
    else:
      raise NotImplementedError(f'Action: {action} is not implemented.')

  def _addRandomEndpoints(self):
    num_endpoints_to_add = random.randrange(0, len(self._inactive_endpoints) + 1)

    # All endpoints active or 0 endpoints chosen, do nothing.
    if not self._inactive_endpoints or not num_endpoints_to_add:
      return

    random.shuffle(self._inactive_endpoints)

    for i in range(num_endpoints_to_add):
      endpoint_to_activate = self._inactive_endpoints.pop()
      self._lb_endpoints.append(endpoint_to_activate)

  def _removeRandomEndpoints(self):
    resources = self._lb_endpoints
    num_endpoints_to_remove = random.randrange(0, len(resources) + 1)

    # All endpoints inactive or 0 endpoints chosen, do nothing.
    if not resources or not num_endpoints_to_remove:
      return

    for i in range(num_endpoints_to_remove):
      index_to_remove = random.randrange(0, len(resources))
      removed_endpoint = resources.pop(index_to_remove)
      self._inactive_endpoints.append(removed_endpoint)

  def getLastMutateActionForTesting(self):
    """Provide the last mutation action that occured for testing."""
    return self._last_mutate_action

  def timeBeforeNextUpdate(self):
    """Provide the time before the next execution."""
    return self._refresh_interval

  def execute(self):
    """Periodically execute this method."""
    self.mutate()
    self._active_config.version_info = str(int(self._active_config.version_info) + 1)
    self.serializeToFile()

  def serializeToFile(self):
    """Serialize current configuration of clusters to output file."""
    contents = self.serialize()
    versioned_file = self._output_file + '.tmp'
    with open(versioned_file, 'wb') as f:
      f.write(contents)
    # Triggers the update to be picked up.
    os.replace(versioned_file, self._output_file)
    logging.info(
        f'Refreshed configuration at {self._output_file} new contents:\n{self._active_config}')

  def serialize(self) -> str:
    """Serialize current configuration of clusters."""
    self._active_config.resources[0].Pack(self._cluster_load_assignment)
    return self._active_config.SerializeToString()


if __name__ == '__main__':
  logging.root.setLevel(logging.INFO)
  logging.info('Parsing dynamic config manager arguments.')

  parser = argparse.ArgumentParser(
      description='Provide a base64 encoded any-wrapped protobuf string of the configuration.')
  parser.add_argument('--config', required=True, help='base64 encoded any-wrapped protobuf string.')
  args = parser.parse_args()

  base64_decoded_any_wrapped = base64.b64decode(args.config, validate=True)
  wrapped_proto = any_pb2.Any()
  wrapped_proto.ParseFromString(base64_decoded_any_wrapped)

  supported_types_to_proto = {
      'type.googleapis.com/nighthawk.DynamicClusterConfigManagerSettings':
          cluster_config_manager_pb2.DynamicClusterConfigManagerSettings,
      'type.googleapis.com/nighthawk.DynamicEndpointsConfigManagerSettings':
          endpoints_config_manager_pb2.DynamicEndpointsConfigManagerSettings
  }

  type_url = wrapped_proto.type_url
  settings = supported_types_to_proto[type_url]()
  wrapped_proto.Unpack(settings)

  supported_types_to_config_manager = {
      'type.googleapis.com/nighthawk.DynamicClusterConfigManagerSettings':
          DynamicClusterConfigManager,
      'type.googleapis.com/nighthawk.DynamicEndpointsConfigManagerSettings':
          DynamicEndpointsConfigManager
  }

  config_manager = supported_types_to_config_manager[type_url](settings)

  logging.info(f'Starting dynamic updates for type:{type_url}. Terminate the process to stop.')
  config_manager.start()
