#include "envoy/api/io_error.h"
#include "envoy/filesystem/filesystem.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/common/exception.h"

#include "external/envoy/test/mocks/filesystem/mocks.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"

#include "test/adaptive_load/minimal_output.h"
#include "test/mocks/adaptive_load/mock_adaptive_load_controller.h"
#include "test/test_common/environment.h"

#include "absl/strings/string_view.h"
#include "adaptive_load/adaptive_load_client_main.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

/**
 * Envoy IO error value to simulate filesystem errors.
 */
class UnknownIoError : public Envoy::Api::IoError {
public:
  IoErrorCode getErrorCode() const override {
    return Envoy::Api::IoError::IoErrorCode::UnknownError;
  }
  std::string getErrorDetails() const override { return "unknown error details"; }
};

/**
 * Creates a minimal valid output that matches test/adaptive_load/test_data/golden_output.textproto.
 *
 * @return AdaptiveLoadSessionOutput
 */
nighthawk::adaptive_load::AdaptiveLoadSessionOutput MakeBasicAdaptiveLoadSessionOutput() {
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output;
  nighthawk::adaptive_load::MetricEvaluation* evaluation =
      output.mutable_adjusting_stage_results()->Add()->add_metric_evaluations();
  evaluation->set_metric_id("com.a/b");
  evaluation->set_metric_value(123);
  return output;
}

TEST(AdaptiveLoadClientMainTest, FailsWithNoInputs) {
  const std::vector<const char*> argv = {
      "executable-name-here",
  };
  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  EXPECT_THROW_WITH_REGEX(AdaptiveLoadClientMain(1, argv.data(), controller, filesystem),
                          Nighthawk::Client::MalformedArgvException, "Required arguments missing");
}

TEST(AdaptiveLoadClientMainTest, FailsIfSpecFileNotSet) {
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
  const std::vector<const char*> argv = {
      "executable-name-here",
      "--output-file",
      outfile.c_str(),
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  EXPECT_THROW_WITH_REGEX(AdaptiveLoadClientMain(3, argv.data(), controller, filesystem),
                          Nighthawk::Client::MalformedArgvException,
                          "Required argument missing: spec-file");
}

TEST(AdaptiveLoadClientMainTest, FailsIfOutputFileNotSet) {
  std::string infile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
  const std::vector<const char*> argv = {
      "executable-name-here",
      "--spec-file",
      infile.c_str(),
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  EXPECT_THROW_WITH_REGEX(AdaptiveLoadClientMain main(3, argv.data(), controller, filesystem),
                          Nighthawk::Client::MalformedArgvException,
                          "Required argument missing: output-file");
}

TEST(AdaptiveLoadClientMainTest, FailsWithNonexistentInputFile) {
  std::string infile = Nighthawk::TestEnvironment::runfilesPath("nonexistent.textproto");
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException,
                          "Failed to read spec textproto file");
}

TEST(AdaptiveLoadClientMainTest, FailsWithUnparseableInputFile) {
  std::string infile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/invalid_session_spec.textproto");
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath("unused.textproto");
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException, "Unable to parse file");
}

TEST(AdaptiveLoadClientMainTest, ExitsProcessWithNonzeroStatusOnAdaptiveControllerError) {
  std::string infile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/valid_session_spec.textproto");
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/nonexistent-dir/out.textproto");
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  MockAdaptiveLoadController controller;
  EXPECT_CALL(controller, PerformAdaptiveLoadSession(_, _))
      .WillOnce(Return(absl::DataLossError("error message")));
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  EXPECT_EQ(main.Run(), 1);
}

TEST(AdaptiveLoadClientMainTest, FailsIfCreatingOutputFileFails) {
  std::string infile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/valid_session_spec.textproto");
  std::string outfile = Nighthawk::TestEnvironment::runfilesPath(
      "test/adaptive_load/test_data/nonexistent-dir/out.textproto");
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file", infile.c_str(), "--output-file", outfile.c_str(),
  };

  MockAdaptiveLoadController controller;
  EXPECT_CALL(controller, PerformAdaptiveLoadSession(_, _))
      .WillOnce(Return(MakeBasicAdaptiveLoadSessionOutput()));
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException, "Unable to open output file");
}

TEST(AdaptiveLoadClientMainTest, FailsIfOpeningOutputFileFails) {
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file",         "in-dummy.textproto",
      "--output-file",        "out-dummy.textproto",
  };

  MockAdaptiveLoadController controller;
  EXPECT_CALL(controller, PerformAdaptiveLoadSession(_, _))
      .WillOnce(Return(MakeBasicAdaptiveLoadSessionOutput()));

  NiceMock<Envoy::Filesystem::MockInstance> filesystem;

  std::string infile_contents =
      Envoy::Filesystem::fileSystemForTest().fileReadToEnd(Nighthawk::TestEnvironment::runfilesPath(
          std::string("test/adaptive_load/test_data/valid_session_spec.textproto")));
  EXPECT_CALL(filesystem, fileReadToEnd(_)).WillOnce(Return(infile_contents));

  auto* mock_file = new NiceMock<Envoy::Filesystem::MockFile>;
  EXPECT_CALL(filesystem, createFile(_))
      .WillOnce(Return(ByMove(std::unique_ptr<NiceMock<Envoy::Filesystem::MockFile>>(mock_file))));

  EXPECT_CALL(*mock_file, open_(_))
      .WillOnce(Return(ByMove(Envoy::Api::IoCallBoolResult(
          false, Envoy::Api::IoErrorPtr(new UnknownIoError(),
                                        [](Envoy::Api::IoError* err) { delete err; })))));

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException, "Unable to open output file");
}

TEST(AdaptiveLoadClientMainTest, FailsIfWritingOutputFileFails) {
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file",         "in-dummy.textproto",
      "--output-file",        "out-dummy.textproto",
  };

  MockAdaptiveLoadController controller;
  EXPECT_CALL(controller, PerformAdaptiveLoadSession(_, _))
      .WillOnce(Return(MakeBasicAdaptiveLoadSessionOutput()));

  NiceMock<Envoy::Filesystem::MockInstance> filesystem;

  std::string infile_contents =
      Envoy::Filesystem::fileSystemForTest().fileReadToEnd(Nighthawk::TestEnvironment::runfilesPath(
          std::string("test/adaptive_load/test_data/valid_session_spec.textproto")));
  EXPECT_CALL(filesystem, fileReadToEnd(_)).WillOnce(Return(infile_contents));

  auto* mock_file = new NiceMock<Envoy::Filesystem::MockFile>;
  EXPECT_CALL(filesystem, createFile(_))
      .WillOnce(Return(ByMove(std::unique_ptr<NiceMock<Envoy::Filesystem::MockFile>>(mock_file))));

  EXPECT_CALL(*mock_file, open_(_))
      .WillOnce(Return(ByMove(Envoy::Api::IoCallBoolResult(
          true, Envoy::Api::IoErrorPtr(nullptr, [](Envoy::Api::IoError*) {})))));
  EXPECT_CALL(*mock_file, write_(_))
      .WillOnce(Return(ByMove(Envoy::Api::IoCallSizeResult(
          -1, Envoy::Api::IoErrorPtr(new UnknownIoError(),
                                     [](Envoy::Api::IoError* err) { delete err; })))));
  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException,
                          "Unable to write to output file");
}

TEST(AdaptiveLoadClientMainTest, FailsIfClosingOutputFileFails) {
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file",         "in-dummy.textproto",
      "--output-file",        "out-dummy.textproto",
  };

  MockAdaptiveLoadController controller;
  EXPECT_CALL(controller, PerformAdaptiveLoadSession(_, _))
      .WillOnce(Return(MakeBasicAdaptiveLoadSessionOutput()));

  NiceMock<Envoy::Filesystem::MockInstance> filesystem;

  std::string infile_contents =
      Envoy::Filesystem::fileSystemForTest().fileReadToEnd(Nighthawk::TestEnvironment::runfilesPath(
          std::string("test/adaptive_load/test_data/valid_session_spec.textproto")));
  EXPECT_CALL(filesystem, fileReadToEnd(_)).WillOnce(Return(infile_contents));

  auto* mock_file = new NiceMock<Envoy::Filesystem::MockFile>;
  EXPECT_CALL(filesystem, createFile(_))
      .WillOnce(Return(ByMove(std::unique_ptr<NiceMock<Envoy::Filesystem::MockFile>>(mock_file))));

  EXPECT_CALL(*mock_file, open_(_))
      .WillOnce(Return(ByMove(Envoy::Api::IoCallBoolResult(
          true, Envoy::Api::IoErrorPtr(nullptr, [](Envoy::Api::IoError*) {})))));
  EXPECT_CALL(*mock_file, write_(_))
      .WillRepeatedly(Invoke([](absl::string_view data) -> Envoy::Api::IoCallSizeResult {
        return Envoy::Api::IoCallSizeResult(
            static_cast<ssize_t>(data.length()),
            Envoy::Api::IoErrorPtr(nullptr, [](Envoy::Api::IoError*) {}));
      }));
  EXPECT_CALL(*mock_file, close_())
      .WillOnce(Return(ByMove(Envoy::Api::IoCallBoolResult(
          false, Envoy::Api::IoErrorPtr(new UnknownIoError(),
                                        [](Envoy::Api::IoError* err) { delete err; })))));

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  EXPECT_THROW_WITH_REGEX(main.Run(), Nighthawk::NighthawkException, "Unable to close output file");
}

TEST(AdaptiveLoadClientMainTest, WritesOutputProtoToFile) {
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file",         "in-dummy.textproto",
      "--output-file",        "out-dummy.textproto",
  };

  MockAdaptiveLoadController controller;
  EXPECT_CALL(controller, PerformAdaptiveLoadSession(_, _))
      .WillOnce(Return(MakeBasicAdaptiveLoadSessionOutput()));

  NiceMock<Envoy::Filesystem::MockInstance> filesystem;

  std::string infile_contents =
      Envoy::Filesystem::fileSystemForTest().fileReadToEnd(Nighthawk::TestEnvironment::runfilesPath(
          std::string("test/adaptive_load/test_data/valid_session_spec.textproto")));
  EXPECT_CALL(filesystem, fileReadToEnd(_)).WillOnce(Return(infile_contents));

  std::string actual_outfile_contents;
  auto* mock_file = new NiceMock<Envoy::Filesystem::MockFile>;
  EXPECT_CALL(filesystem, createFile(_))
      .WillOnce(Return(ByMove(std::unique_ptr<NiceMock<Envoy::Filesystem::MockFile>>(mock_file))));

  EXPECT_CALL(*mock_file, open_(_))
      .WillOnce(Return(ByMove(Envoy::Api::IoCallBoolResult(
          true, Envoy::Api::IoErrorPtr(nullptr, [](Envoy::Api::IoError*) {})))));
  EXPECT_CALL(*mock_file, write_(_))
      .WillRepeatedly(Invoke(
          [&actual_outfile_contents](absl::string_view data) -> Envoy::Api::IoCallSizeResult {
            actual_outfile_contents += data;
            return Envoy::Api::IoCallSizeResult(
                static_cast<ssize_t>(data.length()),
                Envoy::Api::IoErrorPtr(nullptr, [](Envoy::Api::IoError*) {}));
          }));

  EXPECT_CALL(*mock_file, close_())
      .WillOnce(Return(ByMove(Envoy::Api::IoCallBoolResult(
          true, Envoy::Api::IoErrorPtr(nullptr, [](Envoy::Api::IoError*) {})))));

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);
  main.Run();

  std::string golden_output =
      Envoy::Filesystem::fileSystemForTest().fileReadToEnd(Nighthawk::TestEnvironment::runfilesPath(
          std::string("test/adaptive_load/test_data/golden_output.textproto")));
  EXPECT_EQ(actual_outfile_contents, golden_output);
}

TEST(AdaptiveLoadClientMainTest, DefaultsToInsecureConnection) {
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file", "a", "--output-file", "b",
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("insecure"));
}

TEST(AdaptiveLoadClientMainTest, UsesTlsConnectionWhenSpecified) {
  const std::vector<const char*> argv = {
      "executable-name-here", "--use-tls", "--spec-file", "a", "--output-file", "b",
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(6, argv.data(), controller, filesystem);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("TLS"));
}

TEST(AdaptiveLoadClientMainTest, UsesDefaultNighthawkServiceAddress) {
  const std::vector<const char*> argv = {
      "executable-name-here", "--spec-file", "a", "--output-file", "b",
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(5, argv.data(), controller, filesystem);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("localhost:8443"));
}

TEST(AdaptiveLoadClientMainTest, UsesCustomNighthawkServiceAddress) {
  const std::vector<const char*> argv = {
      "executable-name-here",
      "--nighthawk-service-address",
      "1.2.3.4:5678",
      "--spec-file",
      "a",
      "--output-file",
      "b",
  };

  NiceMock<MockAdaptiveLoadController> controller;
  Envoy::Filesystem::Instance& filesystem = Envoy::Filesystem::fileSystemForTest();

  AdaptiveLoadClientMain main(7, argv.data(), controller, filesystem);

  EXPECT_THAT(main.DescribeInputs(), HasSubstr("1.2.3.4:5678"));
}

} // namespace

} // namespace Nighthawk
