#include "nighthawk/user_defined_output/user_defined_output_plugin.h"

#include "external/envoy/source/common/config/utility.h"

#include "source/user_defined_output/user_defined_output_plugin_creator.h"

#include "test/user_defined_output/fake_plugin/fake_user_defined_output.h"
#include "test/user_defined_output/fake_plugin/fake_user_defined_output.pb.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::envoy::config::core::v3::TypedExtensionConfig;
using ::google::protobuf::TextFormat;
using ::nighthawk::FakeUserDefinedOutputConfig;
using ::testing::HasSubstr;

std::pair<TypedExtensionConfig, UserDefinedOutputPluginFactory*>
CreateFactoryConfigPair(const std::string& plugin_name, const std::string& config_textproto) {
  FakeUserDefinedOutputConfig config;
  TextFormat::ParseFromString(config_textproto, &config);

  TypedExtensionConfig typed_config;
  *typed_config.mutable_name() = plugin_name;
  typed_config.mutable_typed_config()->PackFrom(config);

  auto* factory = Envoy::Config::Utility::getAndCheckFactory<UserDefinedOutputPluginFactory>(
      typed_config, false);

  return {typed_config, factory};
}

TEST(CreateUserDefinedOutputPlugins, ReturnsEmptyVectorWhenNoConfigs) {
  std::vector<std::pair<TypedExtensionConfig, UserDefinedOutputPluginFactory*>>
      config_factory_pairs{};
  std::vector<UserDefinedOutputPluginPtr> plugins{};
  EXPECT_EQ(createUserDefinedOutputPlugins(config_factory_pairs, 0), plugins);
}

TEST(CreateUserDefinedOutputPlugins, CreatesPluginsForEachConfig) {
  std::vector<std::pair<TypedExtensionConfig, UserDefinedOutputPluginFactory*>>
      config_factory_pairs{};
  config_factory_pairs.push_back(CreateFactoryConfigPair("nighthawk.fake_user_defined_output",
                                                         "fail_per_worker_output: false"));

  std::vector<UserDefinedOutputPluginPtr> plugins =
      createUserDefinedOutputPlugins(config_factory_pairs, 0);
  EXPECT_EQ(plugins.size(), 1);
  EXPECT_NE(dynamic_cast<FakeUserDefinedOutputPlugin*>(plugins[0].get()), nullptr);

  // TODO(nbperry): Test multiple plugins when multiple plugin types exist.
}

} // namespace
} // namespace Nighthawk
