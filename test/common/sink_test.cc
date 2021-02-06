#include "external/envoy/source/common/common/random_generator.h"

#include "common/sink_impl.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

// Future sink implementations register here for testing top level generic sink behavior.
using SinkTypes = ::testing::Types<FileSinkImpl>;

template <typename T> class TypedSinkTest : public ::testing::Test {
public:
  void SetUp() override { uuid_ = random_.uuid(); }
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
  EXPECT_EQ(status.ok(), true);
  const auto status_or_execution_responses = sink.LoadExecutionResult(this->executionIdForTest());
  ASSERT_EQ(status_or_execution_responses.ok(), true);
  ASSERT_EQ(status_or_execution_responses.value().size(), 1);
  EXPECT_EQ(this->executionIdForTest(), status_or_execution_responses.value()[0].execution_id());
}

TYPED_TEST(TypedSinkTest, LoadNonExisting) {
  TypeParam sink;
  const auto status_or_execution_responses = sink.LoadExecutionResult("key-that-does-not-exist");
  ASSERT_EQ(status_or_execution_responses.ok(), false);
  ASSERT_EQ(status_or_execution_responses.status().code(), absl::StatusCode::kNotFound);
}

TYPED_TEST(TypedSinkTest, EmptyKeyStoreFails) {
  TypeParam sink;
  nighthawk::client::ExecutionResponse result_to_store;
  *(result_to_store.mutable_execution_id()) = "";
  const absl::Status status = sink.StoreExecutionResultPiece(result_to_store);
  ASSERT_EQ(status.ok(), false);
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(status.message(), "Received an empty execution id");
}

TYPED_TEST(TypedSinkTest, EmptyKeyLoadFails) {
  TypeParam sink;
  const auto status_or_execution_responses = sink.LoadExecutionResult("");
  ASSERT_EQ(status_or_execution_responses.ok(), false);
  EXPECT_EQ(status_or_execution_responses.status().code(), absl::StatusCode::kInternal);
  EXPECT_EQ(status_or_execution_responses.status().message(), "Received an empty execution id");
}

TYPED_TEST(TypedSinkTest, Append) {
  TypeParam sink;
  nighthawk::client::ExecutionResponse result_to_store;
  *(result_to_store.mutable_execution_id()) = this->executionIdForTest();
  absl::Status status = sink.StoreExecutionResultPiece(result_to_store);
  EXPECT_EQ(status.ok(), true);
  status = sink.StoreExecutionResultPiece(result_to_store);
  EXPECT_EQ(status.ok(), true);
  const auto status_or_execution_responses = sink.LoadExecutionResult(this->executionIdForTest());
  EXPECT_EQ(status_or_execution_responses.value().size(), 2);
}

TYPED_TEST(TypedSinkTest, MultiPart) {}

TYPED_TEST(TypedSinkTest, BadId) {}

TYPED_TEST(TypedSinkTest, CorruptedFile) {}

} // namespace
} // namespace Nighthawk
