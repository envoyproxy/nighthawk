#include <google/protobuf/util/json_util.h>

#include <chrono>
#include <random>
#include <string>
#include <typeinfo> // std::bad_cast

#include "external/envoy/source/common/protobuf/utility.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/statistic_impl.h"

#include "test/test_common/environment.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {

using MyTypes = Types<SimpleStatistic, InMemoryStatistic, HdrStatistic, StreamingStatistic,
                      CircllhistStatistic>;

template <typename T> class TypedStatisticTest : public Test {};

class Helper {
public:
  /**
   * With 0 significant digits passed, this uses EXPECT_DOUBLE_EQ. Otherwise expectNear
   * will be called with a computed acceptable range based on the number of significant
   * digits and tested_value.
   * @param expected_value the expected value
   * @param tested_value the tested_value
   * @param significant the number of significant digits that should be used to compare values.
   */
  static void expectNear(double expected_value, double tested_value, uint64_t significant) {
    if (significant > 0) {
      EXPECT_NEAR(expected_value, tested_value,
                  std::pow(10, std::ceil(std::log10(tested_value)) - 1 - significant));
    } else {
      EXPECT_DOUBLE_EQ(expected_value, tested_value);
    }
  }
};

TYPED_TEST_SUITE(TypedStatisticTest, MyTypes);

TYPED_TEST(TypedStatisticTest, Simple) {
  TypeParam a;
  TypeParam b;

  std::vector<int> a_values{1, 2, 3};
  std::vector<int> b_values{1234, 6543456, 342335};

  for (int value : a_values) {
    a.addValue(value);
  }
  EXPECT_EQ(3, a.count());
  EXPECT_EQ(1, a.min());
  EXPECT_EQ(3, a.max());

  for (int value : b_values) {
    b.addValue(value);
  }
  EXPECT_EQ(3, b.count());
  EXPECT_EQ(1234, b.min());
  // We substract one from the expected precision with respect to significant digits for
  // HdrHistogram. (More context in comments over at the the HdrStatisticProtoOutputLargeValues test
  // below).
  Helper::expectNear(6543456, b.max(), b.significantDigits() - 1);

  Helper::expectNear(2.0, a.mean(), a.significantDigits());
  Helper::expectNear(0.6666666666666666, a.pvariance(), a.significantDigits());
  Helper::expectNear(0.816496580927726, a.pstdev(), a.significantDigits());

  Helper::expectNear(2295675.0, b.mean(), a.significantDigits());
  Helper::expectNear(9041213360680.666, b.pvariance(), a.significantDigits());
  Helper::expectNear(3006861.0477839955, b.pstdev(), a.significantDigits());

  auto c = a.combine(b);
  EXPECT_EQ(6, c->count());
  EXPECT_EQ(1, c->min());
  Helper::expectNear(6543456, c->max(), c->significantDigits() - 1);
  Helper::expectNear(1147838.5, c->mean(), c->significantDigits());
  Helper::expectNear(5838135311072.917, c->pvariance(), c->significantDigits());
  Helper::expectNear(2416223.357033227, c->pstdev(), c->significantDigits());

  // A reverse combine should be exactly equivalent.
  auto d = b.combine(a);
  EXPECT_EQ(c->count(), d->count());
  EXPECT_EQ(c->min(), d->min());
  EXPECT_EQ(c->max(), d->max());
  EXPECT_EQ(c->mean(), d->mean());
  EXPECT_EQ(c->pvariance(), d->pvariance());
  EXPECT_EQ(c->pstdev(), d->pstdev());
}

TYPED_TEST(TypedStatisticTest, createNewInstanceOfSameType) {
  TypeParam a;
  EXPECT_NE(a.createNewInstanceOfSameType(), nullptr);
}

TYPED_TEST(TypedStatisticTest, Empty) {
  TypeParam a;
  EXPECT_EQ(0, a.count());
  EXPECT_TRUE(std::isnan(a.mean()));
  EXPECT_TRUE(std::isnan(a.pvariance()));
  EXPECT_TRUE(std::isnan(a.pstdev()));
  EXPECT_EQ(a.min(), UINT64_MAX);
  EXPECT_EQ(a.max(), 0);
}

TYPED_TEST(TypedStatisticTest, SingleAndDoubleValue) {
  TypeParam a;

  a.addValue(1);
  EXPECT_EQ(1, a.count());
  Helper::expectNear(1.0, a.mean(), a.significantDigits());
  EXPECT_DOUBLE_EQ(0, a.pvariance());
  EXPECT_DOUBLE_EQ(0, a.pstdev());

  a.addValue(2);
  EXPECT_EQ(2, a.count());
  Helper::expectNear(1.5, a.mean(), a.significantDigits());
  Helper::expectNear(0.25, a.pvariance(), a.significantDigits());
  Helper::expectNear(0.5, a.pstdev(), a.significantDigits());
}

TYPED_TEST(TypedStatisticTest, CatastrophicalCancellation) {
  // From https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
  // Assume that all floating point operations use standard IEEE 754 double-precision arithmetic.
  // Consider the sample (4, 7, 13, 16) from an infinite population. Based on this sample, the
  // estimated population mean is 10, and the unbiased estimate of population variance is 30. Both
  // the naïve algorithm and two-pass algorithm compute these values correctly.
  // Next consider the sample (108 + 4, 108 + 7, 108 + 13, 108 + 16), which gives rise to the same
  // estimated variance as the first sample. The two-pass algorithm computes this variance estimate
  // correctly, but the naïve algorithm returns 29.333333333333332 instead of 30. While this loss of
  // precision may be tolerable and viewed as a minor flaw of the naïve algorithm, further
  // increasing the offset makes the error catastrophic. Consider the sample (109 + 4, 109 + 7, 109
  // + 13, 109 + 16). Again the estimated population variance of 30 is computed correctly by the
  // two-pass algorithm, but the naïve algorithm now computes it as −170.66666666666666. This is a
  // serious problem with naïve algorithm and is due to catastrophic cancellation in the subtraction
  // of two similar numbers at the final stage of the algorithm.
  std::vector<uint64_t> values{4, 7, 13, 16};
  uint64_t exponential = 0;
  for (exponential = 3; exponential < 16; exponential++) {
    TypeParam a;
    double offset = std::pow(10ULL, exponential);
    for (int value : values) {
      a.addValue(offset + value);
    }
    // If an implementation makes this claim, we put it to the test. SimpleStatistic is simple and
    // fast, but starts failing this test when exponential equals 8. HdrStatistic breaks at 5.
    // TODO(oschaaf): evaluate ^^
    if (a.resistsCatastrophicCancellation()) {
      Helper::expectNear(22.5, a.pvariance(), a.significantDigits());
      Helper::expectNear(4.7434164902525691, a.pstdev(), a.significantDigits());
    }
  }
}

TYPED_TEST(TypedStatisticTest, OneMillionRandomSamples) {
  std::mt19937_64 mt(1243);
  // TODO(oschaaf): Actually the range we want to test is a factor 1000 higher, but
  // then catastrophical cancellation make SimpleStatistic fail expectations.
  // For now, we use values that shouldn't trigger the phenomena. Revisit this later.
  std::uniform_int_distribution<uint64_t> dist(1ULL, 1000ULL * 1000 * 60);
  StreamingStatistic referenceStatistic;
  TypeParam testStatistic;

  for (int i = 0; i < 999999; ++i) {
    auto value = dist(mt);
    referenceStatistic.addValue(value);
    testStatistic.addValue(value);
  }
  Helper::expectNear(referenceStatistic.mean(), testStatistic.mean(),
                     testStatistic.significantDigits());
  Helper::expectNear(referenceStatistic.pvariance(), testStatistic.pvariance(),
                     testStatistic.significantDigits());
  Helper::expectNear(referenceStatistic.pstdev(), testStatistic.pstdev(),
                     testStatistic.significantDigits());
}

TYPED_TEST(TypedStatisticTest, ProtoOutput) {
  TypeParam a;

  a.setId("foo");
  a.addValue(6543456);
  a.addValue(342335);

  const nighthawk::client::Statistic proto = a.toProto(Statistic::SerializationDomain::DURATION);

  EXPECT_EQ("foo", proto.id());
  EXPECT_EQ(2, proto.count());
  EXPECT_EQ(std::round(a.mean()), proto.mean().nanos());
  EXPECT_EQ(std::round(a.pstdev()), proto.pstdev().nanos());
}

TYPED_TEST(TypedStatisticTest, ProtoOutputEmptyStats) {
  TypeParam a;
  const nighthawk::client::Statistic proto = a.toProto(Statistic::SerializationDomain::DURATION);

  EXPECT_EQ(proto.count(), 0);
  EXPECT_EQ(proto.mean().nanos(), 0);
  EXPECT_EQ(proto.pstdev().nanos(), 0);
}

TYPED_TEST(TypedStatisticTest, NativeRoundtrip) {
  TypeParam a;

  a.setId("bar");
  a.addValue(6543456);
  a.addValue(342335);
  a.addValue(543);

  const absl::StatusOr<std::unique_ptr<std::istream>> status_or_stream = a.serializeNative();
  if (status_or_stream.ok()) {
    // If the histogram states it implements native serialization/deserialization, put it through
    // a round trip test.
    TypeParam b;
    absl::Status status = b.deserializeNative(*status_or_stream.value());
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(3, b.count());
    EXPECT_EQ(a.count(), b.count());
    EXPECT_EQ(a.mean(), b.mean());
    EXPECT_EQ(a.pstdev(), b.pstdev());
  } else {
    EXPECT_EQ(status_or_stream.status().code(), absl::StatusCode::kUnimplemented);
  }
}

TYPED_TEST(TypedStatisticTest, AttemptsToDeserializeBogusBehaveWell) {
  // Deserializing corrupted data should either result in the statistic reporting
  // it didn't implement deserialization, or having it report an internal failure.
  const std::vector<absl::StatusCode> expected_status_list{absl::StatusCode::kInternal,
                                                           absl::StatusCode::kUnimplemented};
  TypeParam a;
  std::istringstream bogus_input(std::string("BOGUS"));
  const absl::Status status = a.deserializeNative(bogus_input);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(expected_status_list, Contains(status.code()));
}

TYPED_TEST(TypedStatisticTest, StringOutput) {
  TypeParam a;

  a.addValue(6543456);
  a.addValue(342335);

  std::string s = a.toString();
  std::vector<std::string> matches{
      "count: ", "raw_mean: ", "raw_pstdev: ", "raw_min: ", "raw_max: "};
  for (const auto& match : matches) {
    EXPECT_NE(std::string::npos, s.find(match));
  }
}

TYPED_TEST(TypedStatisticTest, IdFieldWorks) {
  TypeParam statistic;
  std::string id = "fooid";

  EXPECT_EQ("", statistic.id());
  statistic.setId(id);
  EXPECT_EQ(id, statistic.id());
}

class StatisticTest : public Test {};

// Note that we explicitly subject SimpleStatistic to the large
// values below, and see a 0 stdev returned.
TEST(StatisticTest, SimpleStatisticProtoOutputLargeValues) {
  SimpleStatistic a;
  uint64_t value = 100ul + 0xFFFFFFFF; // 100 + the max for uint32_t
  a.addValue(value);
  a.addValue(value);
  const nighthawk::client::Statistic proto = a.toProto(Statistic::SerializationDomain::DURATION);

  EXPECT_EQ(proto.count(), 2);
  Helper::expectNear(((1.0 * proto.mean().seconds() * 1000 * 1000 * 1000) + proto.mean().nanos()),
                     value, a.significantDigits() - 1);
  // 0 because std::nan() gets translated to that.
  EXPECT_EQ(proto.pstdev().nanos(), 0);
}

TEST(StatisticTest, HdrStatisticProtoOutputLargeValues) {
  HdrStatistic a;
  uint64_t value = 100ul + 0xFFFFFFFF;
  a.addValue(value);
  a.addValue(value);
  const nighthawk::client::Statistic proto = a.toProto(Statistic::SerializationDomain::DURATION);

  EXPECT_EQ(proto.count(), 2);
  // TODO(oschaaf): hdr doesn't seem to the promised precision in this scenario.
  // We substract one from the indicated significant digits to make this test pass.
  // TODO(oschaaf): revisit this to make sure there's not a different underlying problem.
  Helper::expectNear(((1.0 * proto.mean().seconds() * 1000 * 1000 * 1000) + proto.mean().nanos()),
                     value, a.significantDigits() - 1);
  EXPECT_EQ(proto.pstdev().nanos(), 0);
}

TEST(StatisticTest, StreamingStatProtoOutputLargeValues) {
  StreamingStatistic a;
  uint64_t value = 100ul + 0xFFFFFFFF;
  a.addValue(value);
  a.addValue(value);
  const nighthawk::client::Statistic proto = a.toProto(Statistic::SerializationDomain::DURATION);

  EXPECT_EQ(proto.count(), 2);

  Helper::expectNear(((1.0 * proto.mean().seconds() * 1000 * 1000 * 1000) + proto.mean().nanos()),
                     value, a.significantDigits());

  EXPECT_EQ(proto.pstdev().nanos(), 0);
}

TEST(StatisticTest, CircllhistStatisticProtoOutputLargeValues) {
  CircllhistStatistic statistic;
  uint64_t value = 100ul + 0xFFFFFFFF;
  statistic.addValue(value);
  statistic.addValue(value);
  const nighthawk::client::Statistic proto =
      statistic.toProto(Statistic::SerializationDomain::DURATION);

  EXPECT_EQ(proto.count(), 2);
  Helper::expectNear(Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(proto.mean()), value,
                     statistic.significantDigits());
  EXPECT_EQ(Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(proto.pstdev()), 0);
}

TEST(StatisticTest, HdrStatisticPercentilesProto) {
  nighthawk::client::Statistic parsed_json_proto;
  HdrStatistic statistic;

  for (int i = 1; i <= 10; i++) {
    statistic.addValue(i);
  }

  Envoy::MessageUtil util;
  util.loadFromJson(Envoy::Filesystem::fileSystemForTest().fileReadToEnd(
                        TestEnvironment::runfilesPath("test/test_data/hdr_proto_json.gold")),
                    parsed_json_proto, Envoy::ProtobufMessage::getStrictValidationVisitor());
  const std::string json = util.getJsonStringFromMessage(
      statistic.toProto(Statistic::SerializationDomain::DURATION), true, true);
  const std::string golden_json = util.getJsonStringFromMessage(parsed_json_proto, true, true);
  EXPECT_THAT(statistic.toProto(Statistic::SerializationDomain::DURATION),
              Envoy::ProtoEq(parsed_json_proto))
      << json << "\n"
      << "is not equal to golden file:\n"
      << golden_json;
}

TEST(StatisticTest, CircllhistStatisticPercentilesProto) {
  nighthawk::client::Statistic parsed_json_proto;
  CircllhistStatistic statistic;

  for (int i = 1; i <= 10; i++) {
    statistic.addValue(i);
  }

  Envoy::MessageUtil util;
  util.loadFromJson(Envoy::Filesystem::fileSystemForTest().fileReadToEnd(
                        TestEnvironment::runfilesPath("test/test_data/circllhist_proto_json.gold")),
                    parsed_json_proto, Envoy::ProtobufMessage::getStrictValidationVisitor());
  const std::string json = util.getJsonStringFromMessage(
      statistic.toProto(Statistic::SerializationDomain::DURATION), true, true);
  const std::string golden_json = util.getJsonStringFromMessage(parsed_json_proto, true, true);
  EXPECT_THAT(statistic.toProto(Statistic::SerializationDomain::DURATION),
              Envoy::ProtoEq(parsed_json_proto))
      << json << "\n"
      << "is not equal to golden file:\n"
      << golden_json;
}

TEST(StatisticTest, CombineAcrossTypesFails) {
  HdrStatistic a;
  InMemoryStatistic b;
  StreamingStatistic c;
  CircllhistStatistic d;
  EXPECT_THROW(a.combine(b), std::bad_cast);
  EXPECT_THROW(a.combine(c), std::bad_cast);
  EXPECT_THROW(b.combine(a), std::bad_cast);
  EXPECT_THROW(b.combine(c), std::bad_cast);
  EXPECT_THROW(c.combine(a), std::bad_cast);
  EXPECT_THROW(c.combine(b), std::bad_cast);
  EXPECT_THROW(c.combine(d), std::bad_cast);
  EXPECT_THROW(d.combine(a), std::bad_cast);
}

TEST(StatisticTest, HdrStatisticOutOfRange) {
  HdrStatistic a;
  a.addValue(INT64_MAX);
  EXPECT_EQ(0, a.count());
}

TEST(StatisticTest, NullStatistic) {
  NullStatistic stat;
  EXPECT_EQ(0, stat.count());
  std::string id = "fooid";
  stat.setId(id);
  EXPECT_EQ(id, stat.id());
  stat.addValue(1);
  EXPECT_EQ(0, stat.count());
  EXPECT_EQ(0, stat.max());
  EXPECT_EQ(UINT64_MAX, stat.min());
  EXPECT_EQ(0, stat.mean());
  EXPECT_EQ(0, stat.pvariance());
  EXPECT_EQ(0, stat.pstdev());
  EXPECT_NE(nullptr, stat.combine(stat));
  EXPECT_EQ(0, stat.significantDigits());
  EXPECT_NE(nullptr, stat.createNewInstanceOfSameType());
  const nighthawk::client::Statistic proto = stat.toProto(Statistic::SerializationDomain::RAW);
  EXPECT_EQ(id, proto.id());
  EXPECT_EQ(0, proto.count());
  EXPECT_EQ(0, proto.raw_mean());
  EXPECT_EQ(0, proto.raw_pstdev());
  EXPECT_EQ(0, proto.raw_max());
  EXPECT_EQ(UINT64_MAX, proto.raw_min());
}

using SinkableTypes = Types<SinkableHdrStatistic, SinkableCircllhistStatistic>;

template <typename T> class SinkableStatisticTest : public Test {};

TYPED_TEST_SUITE(SinkableStatisticTest, SinkableTypes);

TYPED_TEST(SinkableStatisticTest, EmptySinkableStatistic) {
  Envoy::Stats::MockIsolatedStatsStore mock_store;
  TypeParam stat(mock_store);
  EXPECT_EQ(0, stat.count());
  EXPECT_TRUE(std::isnan(stat.mean()));
  EXPECT_TRUE(std::isnan(stat.pvariance()));
  EXPECT_TRUE(std::isnan(stat.pstdev()));
  EXPECT_EQ(stat.min(), UINT64_MAX);
  EXPECT_EQ(stat.max(), 0);
  EXPECT_EQ(Envoy::Stats::Histogram::Unit::Unspecified, stat.unit());
  EXPECT_FALSE(stat.used());
  EXPECT_EQ("", stat.name());
  EXPECT_EQ("", stat.tagExtractedName());
  EXPECT_EQ(absl::nullopt, stat.worker_id());
}

TYPED_TEST(SinkableStatisticTest, SimpleSinkableStatistic) {
  Envoy::Stats::MockIsolatedStatsStore mock_store;
  const int worker_id = 0;
  TypeParam stat(mock_store, worker_id);
  const uint64_t sample_value = 123;
  const std::string stat_name = "stat_name";

  EXPECT_CALL(mock_store, deliverHistogramToSinks(_, sample_value)).Times(2);
  stat.recordValue(sample_value);
  stat.addValue(sample_value);
  stat.setId(stat_name);

  EXPECT_EQ(2, stat.count());
  Helper::expectNear(123.0, stat.mean(), stat.significantDigits());
  EXPECT_DOUBLE_EQ(0, stat.pvariance());
  EXPECT_DOUBLE_EQ(0, stat.pstdev());
  EXPECT_EQ(123, stat.min());
  EXPECT_EQ(123, stat.max());
  EXPECT_EQ(Envoy::Stats::Histogram::Unit::Unspecified, stat.unit());
  EXPECT_TRUE(stat.used());
  EXPECT_EQ(stat_name, stat.name());
  EXPECT_EQ("0.stat_name", stat.tagExtractedName());
  EXPECT_TRUE(stat.worker_id().has_value());
  EXPECT_EQ(worker_id, stat.worker_id().value());
}

} // namespace Nighthawk
