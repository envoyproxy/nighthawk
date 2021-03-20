#include <filesystem>
#include <fstream>

#include "external/envoy/source/common/common/random_generator.h"

#include "sink/sink_impl.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

// Future sink implementations register here for testing top level generic sink behavior.
using SinkTypes = testing::Types<FileSinkImpl>;

template <typename T> class TypedSinkTest : public testing::Test {
public:
  void SetUp() override { uuid_ = random_.uuid(); }
  void TearDown() override {
    std::error_code error_code;
    std::filesystem::remove_all(std::filesystem::path("/tmp/nh/" + uuid_), error_code);
  }
  std::string executionIdForTest() const { return uuid_; }

private:
  Envoy::Random::RandomGeneratorImpl random_;
  std::string uuid_;
};

TYPED_TEST_SUITE(TypedSinkTest, SinkTypes);

TYPED_TEST(TypedSinkTest, BasicSaveAndLoad) {
  TypeParam sink;
  nighthawk::client::ExecutionResponse result_to_store;
  *(result_to_store.mutable_execution_id()) = this->executionIdForTest();
  absl::Status status = sink.StoreExecutionResultPiece(result_to_store);
  ASSERT_TRUE(status.ok());
  const auto status_or_execution_responses = sink.LoadExecutionResult(this->executionIdForTest());
  ASSERT_EQ(status_or_execution_responses.ok(), true);
  ASSERT_EQ(status_or_execution_responses.value().size(), 1);
  EXPECT_EQ(this->executionIdForTest(), status_or_execution_responses.value()[0].execution_id());
}

TYPED_TEST(TypedSinkTest, LoadNonExisting) {
  TypeParam sink;
  const auto status_or_execution_responses = sink.LoadExecutionResult(this->executionIdForTest());
  ASSERT_EQ(status_or_execution_responses.ok(), false);
  EXPECT_EQ(status_or_execution_responses.status().code(), absl::StatusCode::kNotFound);
}

TYPED_TEST(TypedSinkTest, EmptyKeyStoreFails) {
  TypeParam sink;
  nighthawk::client::ExecutionResponse result_to_store;
  *(result_to_store.mutable_execution_id()) = "";
  const absl::Status status = sink.StoreExecutionResultPiece(result_to_store);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(), "'' is not a guid: bad string length.");
}

TYPED_TEST(TypedSinkTest, EmptyKeyLoadFails) {
  TypeParam sink;
  const auto status_or_execution_responses = sink.LoadExecutionResult("");
  ASSERT_EQ(status_or_execution_responses.ok(), false);
  EXPECT_EQ(status_or_execution_responses.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status_or_execution_responses.status().message(),
            "'' is not a guid: bad string length.");
}

TYPED_TEST(TypedSinkTest, Append) {
  TypeParam sink;
  nighthawk::client::ExecutionResponse result_to_store;
  *(result_to_store.mutable_execution_id()) = this->executionIdForTest();
  absl::Status status = sink.StoreExecutionResultPiece(result_to_store);
  ASSERT_TRUE(status.ok());
  status = sink.StoreExecutionResultPiece(result_to_store);
  ASSERT_TRUE(status.ok());
  const auto status_or_execution_responses = sink.LoadExecutionResult(this->executionIdForTest());
  EXPECT_EQ(status_or_execution_responses.value().size(), 2);
}

// As of today, we constrain execution id to a guid. This way the file sink implementation
// ensures that it can safely use it to create directories. In the future, other sinks may not
// have to worry about such things. In that case it makes sense to add a validation call
// to the sink interface to make this implementation specific, and make the tests below
// implementation specific too.
TYPED_TEST(TypedSinkTest, BadGuidShortString) {
  TypeParam sink;
  const auto status_or_execution_responses =
      sink.LoadExecutionResult("14e75b2a-3e31-4a62-9279-add1e54091f");
  ASSERT_EQ(status_or_execution_responses.ok(), false);
  EXPECT_EQ(status_or_execution_responses.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status_or_execution_responses.status().message(),
            "'14e75b2a-3e31-4a62-9279-add1e54091f' is not a guid: bad string length.");
}

TYPED_TEST(TypedSinkTest, BadGuidBadDashPlacement) {
  TypeParam sink;
  const auto status_or_execution_responses =
      sink.LoadExecutionResult("14e75b2a3-e31-4a62-9279-add1e54091f9");
  ASSERT_EQ(status_or_execution_responses.ok(), false);
  EXPECT_EQ(status_or_execution_responses.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status_or_execution_responses.status().message(),
            "'14e75b2a3-e31-4a62-9279-add1e54091f9' is not a guid: expectations around '-' "
            "positions not met.");
}

TYPED_TEST(TypedSinkTest, BadGuidInvalidCharacter) {
  TypeParam sink;
  const auto status_or_execution_responses =
      sink.LoadExecutionResult("14e75b2a-3e31-4x62-9279-add1e54091f9");
  ASSERT_EQ(status_or_execution_responses.ok(), false);
  EXPECT_EQ(status_or_execution_responses.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(
      status_or_execution_responses.status().message(),
      "'14e75b2a-3e31-4x62-9279-add1e54091f9' is not a guid: unexpected character encountered.");
}

TEST(FileSinkTest, CorruptedFile) {
  FileSinkImpl sink;
  const std::string execution_id = "14e75b2a-3e31-4162-9279-add1e54091f9";
  std::error_code error_code;
  std::filesystem::remove_all("/tmp/nh/" + execution_id + "/", error_code);
  nighthawk::client::ExecutionResponse result_to_store;
  *(result_to_store.mutable_execution_id()) = execution_id;
  ASSERT_TRUE(sink.StoreExecutionResultPiece(result_to_store).ok());
  auto status = sink.LoadExecutionResult(execution_id);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(status.value().size(), 1);
  {
    std::ofstream outfile;
    outfile.open("/tmp/nh/" + execution_id + "/badfile", std::ios_base::out);
    outfile << "this makes no sense";
  }
  status = sink.LoadExecutionResult(execution_id);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.status().message(),
            "Failed to parse ExecutionResponse "
            "'\"/tmp/nh/14e75b2a-3e31-4162-9279-add1e54091f9/badfile\"'.");
}

} // namespace
} // namespace Nighthawk
