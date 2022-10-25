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

UserDefinedOutputConfigFactoryPair CreateFactoryConfigPair(const std::string& plugin_name,
                                                           const std::string& config_textproto) {
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
  std::vector<UserDefinedOutputConfigFactoryPair> config_factory_pairs{};
  std::vector<UserDefinedOutputNamePluginPair> plugins{};
  EXPECT_EQ(createUserDefinedOutputPlugins(config_factory_pairs, 0), plugins);
}

TEST(CreateUserDefinedOutputPlugins, CreatesPluginsForEachConfig) {
  std::vector<UserDefinedOutputConfigFactoryPair> config_factory_pairs{};
  config_factory_pairs.push_back(CreateFactoryConfigPair("nighthawk.fake_user_defined_output",
                                                         "fail_per_worker_output: false"));

  std::vector<UserDefinedOutputNamePluginPair> plugins =
      createUserDefinedOutputPlugins(config_factory_pairs, 0);
  EXPECT_EQ(plugins.size(), 1);
  EXPECT_EQ(plugins[0].first, "nighthawk.fake_user_defined_output");
  EXPECT_NE(dynamic_cast<FakeUserDefinedOutputPlugin*>(plugins[0].second.get()), nullptr);

  // TODO(dubious90): Test multiple plugins when multiple plugin types exist.
}

} // namespace
} // namespace Nighthawk
