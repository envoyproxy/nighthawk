#include "adaptive_rps/step_controller_impl.h"

#include "adaptive_rps/step_controller_impl.h"
#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/adaptive_rps/benchmark_result.pb.h"
#include "api/adaptive_rps/metric_spec.pb.h"
#include "api/adaptive_rps/step_controller_impl.pb.h"
#include "envoy/registry/registry.h"

namespace Nighthawk {
namespace AdaptiveRps {

namespace {

using nighthawk::adaptive_rps::BenchmarkResult;
using nighthawk::adaptive_rps::BinarySearchStepControllerConfig;
using nighthawk::adaptive_rps::LinearSearchStepControllerConfig;
using nighthawk::adaptive_rps::MetricEvaluation;
using nighthawk::adaptive_rps::UNKNOWN_THRESHOLD_STATUS;
using nighthawk::adaptive_rps::WITHIN_THRESHOLD;

template <typename T> inline void ClampBelow(T* value, T minimum) {
  if (*value < minimum) {
    *value = minimum;
  }
}

template <typename T> inline void ClampAbove(T* value, T maximum) {
  if (*value > maximum) {
    *value = maximum;
  }
}

template <typename T> inline void Clamp(T* value, T minimum, T maximum) {
  ClampBelow(value, minimum);
  ClampAbove(value, maximum);
}

double TotalWeightedScore(const BenchmarkResult& benchmark_result) {
  double score = 0;
  double total_weight = 0;
  for (const MetricEvaluation& evaluation : benchmark_result.metric_evaluations()) {
    if (evaluation.threshold_status() == UNKNOWN_THRESHOLD_STATUS) {
      score += evaluation.threshold_score() * evaluation.metric_spec().weight();
    } else {
      score += (evaluation.threshold_status() == WITHIN_THRESHOLD ? 1.0 : -1.0) *
               evaluation.metric_spec().weight();
    }

    total_weight += evaluation.metric_spec().weight();
  }
  return score / total_weight;
}

} // namespace

std::string LinearSearchStepControllerConfigFactory::name() const { return "linear-search"; }

Envoy::ProtobufTypes::MessagePtr LinearSearchStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_rps::LinearSearchStepControllerConfig>();
}

StepControllerPtr LinearSearchStepControllerConfigFactory::createStepController(
    const Envoy::Protobuf::Message& config_any) {
  const LinearSearchStepControllerConfig& config =
      dynamic_cast<const LinearSearchStepControllerConfig&>(config_any);
  return std::make_unique<LinearSearchStepController>(config);
}

REGISTER_FACTORY(LinearSearchStepControllerConfigFactory, StepControllerConfigFactory);

LinearSearchStepController::LinearSearchStepController(
    const LinearSearchStepControllerConfig& config)
    : config_{config}, current_rps_{config.minimum_rps()}, latest_cycle_healthy_{false},
      reached_unhealthy_rps_{false} {}

int LinearSearchStepController::GetCurrentRps() const { return current_rps_; }

bool LinearSearchStepController::IsConverged() const {
  return latest_cycle_healthy_ && reached_unhealthy_rps_;
}

void LinearSearchStepController::UpdateAndRecompute(
    const nighthawk::adaptive_rps::BenchmarkResult& benchmark_result) {
  double score = TotalWeightedScore(benchmark_result);
  if (score < 0.0) {
    latest_cycle_healthy_ = false;
    reached_unhealthy_rps_ = true;
  } else {
    latest_cycle_healthy_ = true;
  }
  current_rps_ += config_.step() * score;
  Clamp(&current_rps_, config_.minimum_rps(), config_.maximum_rps());
}

Envoy::ProtobufTypes::MessagePtr BinarySearchStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_rps::BinarySearchStepControllerConfig>();
}

std::string BinarySearchStepControllerConfigFactory::name() const { return "binary-search"; }

StepControllerPtr BinarySearchStepControllerConfigFactory::createStepController(
    const Envoy::Protobuf::Message& config_any) {
  const BinarySearchStepControllerConfig& config =
      dynamic_cast<const BinarySearchStepControllerConfig&>(config_any);
  return std::make_unique<BinarySearchStepController>(config);
}

REGISTER_FACTORY(BinarySearchStepControllerConfigFactory, StepControllerConfigFactory);

BinarySearchStepController::BinarySearchStepController(
    const BinarySearchStepControllerConfig& config)
    : config_{config}, bottom_rps_{config_.minimum_rps()}, top_rps_{config_.maximum_rps()},
      previous_rps_{-1}, current_rps_{(top_rps_ + bottom_rps_) / 2} {}

int BinarySearchStepController::GetCurrentRps() const { return current_rps_; }

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
