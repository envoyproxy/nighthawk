"""Component that handles dynamic configuration of the Envoy proxy."""

import json
import yaml
from enum import Enum
from google.protobuf import json_format

from abc import ABC, abstractmethod
import envoy.config.cluster.v3.cluster_pb2 as cluster_pb2
import envoy.service.discovery.v3.discovery_pb2 as discovery_pb2
import envoy.extensions.upstreams.http.v3.http_protocol_options_pb2 as http_protocol_options_pb2
import envoy.extensions.transport_sockets.tls.v3.tls_pb2 as tls_pb2
import nighthawk.api.configuration.cluster_config_manager_pb2 as cluster_manager_pb2

import random

from google.protobuf import json_format
import argparse
import time
import base64
import logging
import os


class DynamicConfigManager(ABC):
  """Base class for Dynamic configuration components."""

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

  @abstractmethod
  def timeBeforeNextUpdate(self):
    """Provide the time before the next execution."""
    pass

  @abstractmethod
  def execute(self):
    """Periodically execute this method."""
    pass


class DynamicClusterConfigManager(DynamicConfigManager):
  """Encapsulates dynamic cluster configuration."""

  class Action(Enum):
    """Enum class for mutation actions to configuration."""

    ADD = 1
    REMOVE = 2

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

  mutate_actions = list(Action)

  def __init__(self, config: cluster_manager_pb2.DynamicClusterConfigManagerSettings):
    """Create a DynamicClusterConfigManager."""
    DynamicConfigManager.__init__(self)
    self.active_config = discovery_pb2.DiscoveryResponse()
    self.active_config.version_info = "1"
    self.inactive_clusters = []
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

    # Randomly assign clusters to either inactive or active.
    while clusters:
      add_as_active_cluster = random.choice([True, False])
      if add_as_active_cluster:
        self.active_config.resources.add().Pack(clusters.pop())
      else:
        self.inactive_clusters.append(clusters.pop())
    self.output_file = config.output_file if config.output_file else "new_cds.pb"
    # TODO(kbaichoo): Handle nanos get ignored
    self.refresh_interval = config.refresh_interval.ToSeconds()
    # Tracks the last mutated action. Used for testing.
    self.last_mutate_action = None

  def mutate(self):
    """Invoke to randomly mutate the current cluster configuration."""
    action = random.choice(self.mutate_actions)
    self.last_mutate_action = action
    if action == self.__class__.Action.ADD:
      # Randomly pick a number of clusters to add
      num_clusters_to_add = random.randrange(0, len(self.inactive_clusters) + 1)

      # All clusters active or 0 clusters choosen, do nothing.
      if not self.inactive_clusters or not num_clusters_to_add:
        return

      random.shuffle(self.inactive_clusters)

      for i in range(num_clusters_to_add):
        self.active_config.resources.add().Pack(self.inactive_clusters.pop())

    elif action == self.__class__.Action.REMOVE:
      resources = self.active_config.resources
      num_clusters_to_remove = random.randrange(0, len(resources) + 1)

      # All clusters inactive or 0 clusters choosen, do nothing.
      if not resources or not num_clusters_to_remove:
        return

      for i in range(num_clusters_to_remove):
        any_resource = resources.pop(random.randrange(0, len(resources)))
        unpacked_cluster = cluster_pb2.Cluster()
        any_resource.Unpack(unpacked_cluster)
        self.inactive_clusters.append(unpacked_cluster)
    else:
      raise NotImplementedError(f'Action: {action} is not implemented.')

  def _last_mutate_action(self):
    return self.last_mutate_action

  def timeBeforeNextUpdate(self):
    """Provide the time before the next execution."""
    return self.refresh_interval

  def execute(self):
    """Periodically execute this method."""
    self.mutate()
    self.active_config.version_info = str(int(self.active_config.version_info) + 1)
    self.serializeToFile()

  def serializeToFile(self):
    """Serialize current configuration of clusters to output file."""
    # TODO(kbaichoo): Log the new contents in human readable instead and remove
    # versioned files.
    # Version files to simplify post-test debugging.
    contents = self.active_config.SerializeToString()
    versioned_file = self.output_file + self.active_config.version_info
    with open(versioned_file, 'wb') as f:
      f.write(contents)
    # Triggers the update to be picked up.
    os.replace(versioned_file, self.output_file)
    logging.info(f'Refreshed configuration at {self.output_file} new contents:\n{contents}')

  def serialize(self) -> str:
    """Serialize current configuration of clusters."""
    return self.active_config.SerializeToString()


if __name__ == '__main__':
  logging.root.setLevel(logging.INFO)
  logging.info('Parsing dynamic config manager arguments.')

  parser = argparse.ArgumentParser(
      description='Provide a base64 encoded protobuf string of the configuration.')
  parser.add_argument('--config', required=True, help='base64 encoded protobuf string.')
  args = parser.parse_args()

  base64_decoded = base64.b64decode(args.config, validate=True)
  settings = cluster_manager_pb2.DynamicClusterConfigManagerSettings()
  settings.ParseFromString(base64_decoded)

  mgr = DynamicClusterConfigManager(settings)
  logging.info('Starting dynamic updates. Terminate the process to stop.')
  mgr.start()
