#pragma once

#include <memory>

#include "nighthawk/user_defined_output/user_defined_output_plugin.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy_api/envoy/config/core/v3/extension.pb.h"

namespace Nighthawk {

/**
 * @brief Creates the User Defined Output Plugins for a set of configs.
 *
 * @param configs the TypedExtensionConfigs, used to identify the plugin factories and to configure
 * each plugin created.
 * @param worker_number which worker these plugins will be associated with.
 *
 * @return std::vector<UserDefinedOutputPluginPtr> the created plugins.
 * @throws EnvoyException if the config is invalid or couldn't find a corresponding User Defined
 * Output Plugin.
 */
std::vector<UserDefinedOutputNamePluginPair>
createUserDefinedOutputPlugins(std::vector<UserDefinedOutputConfigFactoryPair>& factory_pairs,
                               int worker_number);

} // namespace Nighthawk
