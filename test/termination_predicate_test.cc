#include <chrono>

#include "external/envoy/test/mocks/event/mocks.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/test_common/simulated_time_system.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/termination_predicate_impl.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {

class TerminationPredicateTest : public Test {
public:
  TerminationPredicateTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}

  Envoy::Api::ApiPtr api_;
  Envoy::Event::SimulatedTimeSystem time_system;
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
};

TEST_F(TerminationPredicateTest, DurationTerminationPredicateImplTest) {
  const auto duration = 100us;
  DurationTerminationPredicateImpl pred(time_system, duration, time_system.monotonicTime());
  EXPECT_EQ(pred.evaluate(), TerminationPredicate::Status::PROCEED);
  // move to the edge.
  time_system.sleep(duration);
  EXPECT_EQ(pred.evaluate(), TerminationPredicate::Status::PROCEED);
  // move past the edge, we expect the predicate to return TERMINATE.
  time_system.sleep(1us);
  EXPECT_EQ(pred.evaluate(), TerminationPredicate::Status::TERMINATE);
}

TEST_F(TerminationPredicateTest, StatsCounterAbsoluteThresholdTerminationPredicateImpl) {
  auto& counter = stats_store_.counter("foo");
  const TerminationPredicate::Status termination_status = TerminationPredicate::Status::FAIL;
  StatsCounterAbsoluteThresholdTerminationPredicateImpl pred(counter, 0, termination_status);
  EXPECT_EQ(pred.evaluate(), TerminationPredicate::Status::PROCEED);
  counter.inc();
  EXPECT_EQ(pred.evaluate(), termination_status);
}

TEST_F(TerminationPredicateTest, LinkedPredicates) {
  auto& fail_counter = stats_store_.counter("counter-associated-to-fail");
  auto& terminate_counter = stats_store_.counter("counter-associated-to-terminate");
  StatsCounterAbsoluteThresholdTerminationPredicateImpl fail_pred(
      fail_counter, 0, TerminationPredicate::Status::FAIL);
  fail_pred.link(std::make_unique<StatsCounterAbsoluteThresholdTerminationPredicateImpl>(
      terminate_counter, 0, TerminationPredicate::Status::TERMINATE));

  EXPECT_EQ(fail_pred.evaluateChain(), TerminationPredicate::Status::PROCEED);

  fail_counter.inc();
  EXPECT_EQ(fail_pred.evaluateChain(), TerminationPredicate::Status::FAIL);

  // We expect linked child predicates to be evaluated first. Hence, bumping the
  // termination counter ought to make the linked child return its terminal status,
  // which is TERMINATE.
  terminate_counter.inc();
  EXPECT_EQ(fail_pred.evaluateChain(), TerminationPredicate::Status::TERMINATE);
}

TEST_F(TerminationPredicateTest, AppendToChain) {
  auto& foo_counter = stats_store_.counter("foo");
  foo_counter.inc();
  StatsCounterAbsoluteThresholdTerminationPredicateImpl predicate(
      foo_counter, 1, TerminationPredicate::Status::TERMINATE);
  // The counter doesn't exceed the predicate threshold, so we shouldn't see TERMINATE
  EXPECT_EQ(predicate.evaluateChain(), TerminationPredicate::Status::PROCEED);
  auto child_predicate = std::make_unique<StatsCounterAbsoluteThresholdTerminationPredicateImpl>(
      foo_counter, 0, TerminationPredicate::Status::FAIL);
  EXPECT_EQ(child_predicate.get(), &(predicate.appendToChain(std::move(child_predicate))));
  // This ought to evaluate to FAIL as the counter threshold is exceeded.
  EXPECT_EQ(predicate.evaluateChain(), TerminationPredicate::Status::FAIL);
}

} // namespace Nighthawk
