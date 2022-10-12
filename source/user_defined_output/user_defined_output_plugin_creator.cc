#include "source/user_defined_output/user_defined_output_plugin_creator.h"

#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/config/utility.h"

namespace Nighthawk {

using envoy::config::core::v3::TypedExtensionConfig;

std::vector<UserDefinedOutputPluginPtr> createUserDefinedOutputPlugins(
    std::vector<std::pair<TypedExtensionConfig, UserDefinedOutputPluginFactory*>>&
        factory_config_pairs,
    int worker_number) {
  std::vector<UserDefinedOutputPluginPtr> plugins;

  for (std::pair<TypedExtensionConfig, UserDefinedOutputPluginFactory*>& pair :
       factory_config_pairs) {
    WorkerMetadata metadata;
    metadata.worker_number = worker_number;
    TypedExtensionConfig config = pair.first;
    UserDefinedOutputPluginFactory* factory = pair.second;
    plugins.push_back(factory->createUserDefinedOutputPlugin(config.typed_config(), metadata));
  }

  return plugins;
}

} // namespace Nighthawk
