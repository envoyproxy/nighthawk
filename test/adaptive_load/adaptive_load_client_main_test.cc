#include <chrono>
#include <iostream>

#include "envoy/config/core/v3/base.pb.h"
#include "envoy/filesystem/filesystem.h"
#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"
#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/source/common/protobuf/utility.h"
#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/input_variable_setter_impl.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/adaptive_load/metrics_plugin_impl.pb.h"
#include "api/adaptive_load/scoring_function_impl.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"
#include "api/client/service.pb.h"
#include "api/client/service_mock.grpc.pb.h"

#include "common/statistic_impl.h"
#include "common/version_info.h"

#include "client/output_collector_impl.h"
#include "client/output_formatter_impl.h"

#include "test/adaptive_load/utility.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "adaptive_load/adaptive_load_client_main.h"
#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_util.h"
#include "adaptive_load/step_controller_impl.h"
#include "grpcpp/test/mock_stream.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SetArgPointee;

TEST(AdaptiveLoadClientMainTest, FailsWithNoInputs) {
  const char* const argv[] = {
      "executable-name-here",
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  EXPECT_THROW_WITH_REGEX(AdaptiveLoadClientMain(1, argv, filesystem, time_source),
                          Nighthawk::Client::MalformedArgvException, "Required arguments missing");
}

TEST(AdaptiveLoadClientMainTest, FailsIfSpecFileNotSet) {
  std::string outfile = Envoy::TestEnvironment::runfilesPath(std::string("nonexistent.textproto"));
  const char* const argv[] = {
      "executable-name-here",
      "--output-file",
      outfile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  EXPECT_THROW_WITH_REGEX(AdaptiveLoadClientMain(3, argv, filesystem, time_source),
                          Nighthawk::Client::MalformedArgvException,
                          "Required argument missing: spec-file");
}

TEST(AdaptiveLoadClientMainTest, FailsIfOutputFileNotSet) {
  std::string infile = Envoy::TestEnvironment::runfilesPath(std::string("nonexistent.textproto"));
  const char* const argv[] = {
      "executable-name-here",
      "--spec-file",
      infile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  EXPECT_THROW_WITH_REGEX(AdaptiveLoadClientMain main(3, argv, filesystem, time_source),
                          Nighthawk::Client::MalformedArgvException,
                          "Required argument missing: output-file");
}

TEST(AdaptiveLoadClientMainTest, FailsWithNonexistentInputFile) {
  std::string infile = Envoy::TestEnvironment::runfilesPath(std::string("nonexistent.textproto"));
  std::string outfile = Envoy::TestEnvironment::runfilesPath(std::string("nonexistent.textproto"));
  const char* const argv[] = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);
  EXPECT_THROW_WITH_REGEX(main.run(), Nighthawk::NighthawkException,
                          "Failed to read spec textproto file");
}

TEST(AdaptiveLoadClientMainTest, FailsWithUnparseableInputFile) {
  std::string infile = Envoy::TestEnvironment::runfilesPath(
      std::string("test/adaptive_load/test_data/invalid_session_spec.textproto"));
  std::string outfile = Envoy::TestEnvironment::runfilesPath(std::string("nonexistent.textproto"));
  const char* const argv[] = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);
  EXPECT_THROW_WITH_REGEX(main.run(), Nighthawk::NighthawkException, "Unable to parse file");
}

TEST(AdaptiveLoadClientMainTest, FailsWithUnwritableOutputFile) {
  std::string infile = Envoy::TestEnvironment::runfilesPath(
      std::string("test/adaptive_load/test_data/valid_session_spec.textproto"));
  std::string outfile = Envoy::TestEnvironment::runfilesPath(
      std::string("test/adaptive_load/nonexistent-dir/out.textproto"));
  const char* const argv[] = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);
  EXPECT_THROW_WITH_REGEX(main.run(), Nighthawk::NighthawkException, "Unable to .* output file");
}

} // namespace

} // namespace Nighthawk