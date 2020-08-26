#include "envoy/filesystem/filesystem.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/common/exception.h"

#include "api/adaptive_load/benchmark_result.pb.h"

#include "common/filesystem/file_shared_impl.h" // fails check_format

// #include "external/envoy/source/common/filesystem/file_shared_impl.h" // check_format possible
// fix

#include "external/envoy/test/mocks/filesystem/mocks.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/adaptive_load/adaptive_load.pb.h"

#include "test/adaptive_load/fake_time_source.h"
#include "test/adaptive_load/minimal_output.h"
#include "test/test_common/environment.h"

#include "absl/strings/string_view.h"
#include "adaptive_load/adaptive_load_client_main.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

absl::StatusOr<nighthawk::adaptive_load::AdaptiveLoadSessionOutput>
PerformAdaptiveLoadSession(nighthawk::client::NighthawkService::StubInterface*,
                           const nighthawk::adaptive_load::AdaptiveLoadSessionSpec&,
                           Envoy::TimeSource&) {
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output;
  nighthawk::adaptive_load::MetricEvaluation* evaluation =
      output.mutable_adjusting_stage_results()->Add()->add_metric_evaluations();
  evaluation->set_metric_id("com.a/b");
  evaluation->set_metric_value(123);
  return output;
}

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::NiceMock;
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
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
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
  std::string infile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
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
  std::string infile = Nighthawk::TestEnvironment::runfilesPath("nonexistent.textproto");
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
  const char* const argv[] = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException,
                          "Failed to read spec textproto file");
}

TEST(AdaptiveLoadClientMainTest, FailsWithUnparseableInputFile) {
  std::string infile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/invalid_session_spec.textproto");
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
  const char* const argv[] = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException, "Unable to parse file");
}

TEST(AdaptiveLoadClientMainTest, FailsWithUnwritableOutputFile) {
  std::string infile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/valid_session_spec.textproto");
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/nonexistent-dir/out.textproto");
  const char* const argv[] = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException, "Unable to open output file");
}

TEST(AdaptiveLoadClientMainTest, WritesOutputProtoToFile) {
  const char* const argv[] = {
      "executable-name-here", "--spec-file",         "in-dummy.textproto",
      "--output-file",        "out-dummy.textproto",
  };

  FakeIncrementingMonotonicTimeSource time_source;

  NiceMock<Envoy::Filesystem::MockInstance> filesystem;

  std::string infile_contents =
      Envoy::Filesystem::fileSystemForTest().fileReadToEnd(Nighthawk::TestEnvironment::runfilesPath(
          std::string("test/adaptive_load/test_data/valid_session_spec.textproto")));
  EXPECT_CALL(filesystem, fileReadToEnd(_)).WillOnce(Return(infile_contents));

  std::string actual_outfile_contents;
  NiceMock<Envoy::Filesystem::MockFile>* file = new NiceMock<Envoy::Filesystem::MockFile>;
  EXPECT_CALL(filesystem, createFile(_))
      .WillOnce(Return(ByMove(std::unique_ptr<NiceMock<Envoy::Filesystem::MockFile>>(file))));

  EXPECT_CALL(*file, open_(_))
      .WillOnce(Return(ByMove(Envoy::Filesystem::resultSuccess<bool>(true))));
  EXPECT_CALL(*file, write_(_))
      .WillRepeatedly(Invoke(
          [&actual_outfile_contents](absl::string_view data) -> Envoy::Api::IoCallSizeResult {
            actual_outfile_contents += data;
            return Envoy::Filesystem::resultSuccess<ssize_t>(static_cast<ssize_t>(data.length()));
          }));

  EXPECT_CALL(*file, close_())
      .WillOnce(Return(ByMove(Envoy::Filesystem::resultSuccess<bool>(true))));

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);
  main.Run();

  std::string golden_output =
      Envoy::Filesystem::fileSystemForTest().fileReadToEnd(Nighthawk::TestEnvironment::runfilesPath(
          std::string("test/adaptive_load/test_data/golden_output.textproto")));
  EXPECT_EQ(actual_outfile_contents, golden_output);
}

TEST(AdaptiveLoadClientMainTest, DefaultsToInsecureConnection) {
  const char* const argv[] = {
      "executable-name-here", "--spec-file", "a", "--output-file", "b",
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("insecure"));
}

TEST(AdaptiveLoadClientMainTest, UsesTlsConnectionWhenSpecified) {
  const char* const argv[] = {
      "executable-name-here", "--use-tls", "--spec-file", "a", "--output-file", "b",
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(6, argv, filesystem, time_source);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("TLS"));
}

TEST(AdaptiveLoadClientMainTest, UsesDefaultNighthawkServiceAddress) {
  const char* const argv[] = {
      "executable-name-here", "--spec-file", "a", "--output-file", "b",
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(5, argv, filesystem, time_source);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("localhost:8443"));
}

TEST(AdaptiveLoadClientMainTest, UsesCustomNighthawkServiceAddress) {
  const char* const argv[] = {
      "executable-name-here",
      "--nighthawk-service-address",
      "1.2.3.4:5678",
      "--spec-file",
      "a",
      "--output-file",
      "b",
  };

  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();
  FakeIncrementingMonotonicTimeSource time_source;

  AdaptiveLoadClientMain main(7, argv, filesystem, time_source);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("1.2.3.4:5678"));
}

} // namespace

} // namespace Nighthawk