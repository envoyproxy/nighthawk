#include "adaptive_rps/step_controller_impl.h"

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/adaptive_rps/benchmark_result.pb.h"
#include "api/adaptive_rps/metric_spec.pb.h"
#include "api/adaptive_rps/step_controller_impl.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

namespace {

using nighthawk::adaptive_rps::BenchmarkResult;
using nighthawk::adaptive_rps::BinarySearchStepControllerConfig;
using nighthawk::adaptive_rps::LinearSearchStepControllerConfig;
using nighthawk::adaptive_rps::MetricEvaluation;
using nighthawk::adaptive_rps::UNKNOWN_THRESHOLD_STATUS;
using nighthawk::adaptive_rps::WITHIN_THRESHOLD;

// Ensures that *value is not below minimum.
template <typename T> inline void ClampBelow(T* value, T minimum) {
  if (*value < minimum) {
    *value = minimum;
  }
}

// Ensures that *value is not above maximum.
template <typename T> inline void ClampAbove(T* value, T maximum) {
  if (*value > maximum) {
    *value = maximum;
  }
}

// Ensures that *value is between minimum and maximum, inclusive.
template <typename T> inline void Clamp(T* value, T minimum, T maximum) {
  ClampBelow(value, minimum);
  ClampAbove(value, maximum);
}

// Adds all collected metric results according to their weights, counting within threshold as 1.0
// and outside threshold as -1.0. Output ranges from -1.0 to 1.0.
double TotalWeightedScore(const BenchmarkResult& benchmark_result) {
  double score = 0.0;
  double total_weight = 0.0;
  for (const MetricEvaluation& evaluation : benchmark_result.metric_evaluations()) {
    if (!(evaluation.has_threshold_spec() && evaluation.has_threshold_check_result())) {
      // Metric was recorded for display purposes only.
      continue;
    }
    // Either all weights or no weights will be set. If no weights are set, all are equal.
    double weight = evaluation.threshold_spec().has_weight()
                        ? evaluation.threshold_spec().weight().value()
                        : 1.0;
    if (evaluation.threshold_check_result().simple_threshold_status() == UNKNOWN_THRESHOLD_STATUS) {
      score += weight * evaluation.threshold_check_result().threshold_score();
    } else {
      score += weight *
               (evaluation.threshold_check_result().simple_threshold_status() == WITHIN_THRESHOLD
                    ? 1.0
                    : -1.0);
    }
    total_weight += weight;
  }
  return score / total_weight;
}

} // namespace

std::string LinearSearchStepControllerConfigFactory::name() const { return "linear-search"; }

Envoy::ProtobufTypes::MessagePtr LinearSearchStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<LinearSearchStepControllerConfig>();
}

StepControllerPtr LinearSearchStepControllerConfigFactory::createStepController(
    const Envoy::Protobuf::Message& message) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  LinearSearchStepControllerConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<LinearSearchStepController>(config);
}

REGISTER_FACTORY(LinearSearchStepControllerConfigFactory, StepControllerConfigFactory);

LinearSearchStepController::LinearSearchStepController(
    const LinearSearchStepControllerConfig& config)
    : config_{config}, current_rps_{config.minimum_rps()}, latest_cycle_healthy_{false},
      reached_unhealthy_rps_{false} {}

unsigned int LinearSearchStepController::GetCurrentRps() const { return current_rps_; }

bool LinearSearchStepController::IsConverged() const {
  return latest_cycle_healthy_ && reached_unhealthy_rps_;
}

void LinearSearchStepController::UpdateAndRecompute(const BenchmarkResult& benchmark_result) {
  double score = TotalWeightedScore(benchmark_result);
  if (score < 0.0) {
    latest_cycle_healthy_ = false;
    reached_unhealthy_rps_ = true;
  } else {
    latest_cycle_healthy_ = true;
  }
  current_rps_ += config_.rps_step() * score;
  Clamp(&current_rps_, config_.minimum_rps(), config_.maximum_rps());
}

Envoy::ProtobufTypes::MessagePtr BinarySearchStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<BinarySearchStepControllerConfig>();
}

std::string BinarySearchStepControllerConfigFactory::name() const { return "binary-search"; }

StepControllerPtr BinarySearchStepControllerConfigFactory::createStepController(
    const Envoy::Protobuf::Message& message) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  BinarySearchStepControllerConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<BinarySearchStepController>(config);
}

REGISTER_FACTORY(BinarySearchStepControllerConfigFactory, StepControllerConfigFactory);

BinarySearchStepController::BinarySearchStepController(
    const BinarySearchStepControllerConfig& config)
    : config_{config}, bottom_rps_{config_.minimum_rps()}, top_rps_{config_.maximum_rps()},
      previous_rps_{0}, current_rps_{(top_rps_ + bottom_rps_) / 2} {}

unsigned int BinarySearchStepController::GetCurrentRps() const { return current_rps_; }

bool BinarySearchStepController::IsConverged() const { return previous_rps_ == current_rps_; }

void BinarySearchStepController::UpdateAndRecompute(const BenchmarkResult& benchmark_result) {
  double score = TotalWeightedScore(benchmark_result);
  if (score < 0.0) {
    top_rps_ = current_rps_;
  } else {
    bottom_rps_ = current_rps_;
  }
  previous_rps_ = current_rps_;
  current_rps_ = (bottom_rps_ + top_rps_) / 2;
  Clamp(&current_rps_, config_.minimum_rps(), config_.maximum_rps());
}

} // namespace AdaptiveRps
} // namespace Nighthawk
