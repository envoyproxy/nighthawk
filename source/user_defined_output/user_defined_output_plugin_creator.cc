#include "source/user_defined_output/user_defined_output_plugin_creator.h"

#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/config/utility.h"

namespace Nighthawk {

using envoy::config::core::v3::TypedExtensionConfig;

absl::StatusOr<std::vector<UserDefinedOutputNamePluginPair>> createUserDefinedOutputPlugins(
    std::vector<UserDefinedOutputConfigFactoryPair>& factory_config_pairs, int worker_number) {
  std::vector<UserDefinedOutputNamePluginPair> plugins;

  for (auto& pair : factory_config_pairs) {
    WorkerMetadata metadata;
    metadata.worker_number = worker_number;
    TypedExtensionConfig config = pair.first;
    UserDefinedOutputPluginFactory* factory = pair.second;

    absl::StatusOr<UserDefinedOutputPluginPtr> plugin =
        factory->createUserDefinedOutputPlugin(config.typed_config(), metadata);
    if (!plugin.ok()) {
      return plugin.status();
    }

    UserDefinedOutputNamePluginPair name_plugin_pair;
    name_plugin_pair.first = factory->name();
    name_plugin_pair.second = std::move(*plugin);
    ;
    plugins.emplace_back(std::move(name_plugin_pair));
  }

  return plugins;
}

} // namespace Nighthawk
