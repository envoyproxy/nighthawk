#pragma once

#include "nighthawk/adaptive_rps/step_controller.h"

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/adaptive_rps/benchmark_result.pb.h"
#include "api/adaptive_rps/step_controller_impl.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

// A StepController that increases the RPS by a fixed step size until metrics go out of threshold,
// then backs off one step. Advantages: Simplicity. Approaches the optimal RPS cautiously from
// below, without overwhelming the system under test. Disadvantage: If the fixed step size is low
// enough to provide good resolution in the answer, it can take many steps to ramp up the RPS to the
// optimal level.
class LinearSearchStepController : public StepController {
public:
  explicit LinearSearchStepController(
      const nighthawk::adaptive_rps::LinearSearchStepControllerConfig& config);

  unsigned int GetCurrentRps() const override;
  bool IsConverged() const override;
  void UpdateAndRecompute(const nighthawk::adaptive_rps::BenchmarkResult& result) override;

private:
  const nighthawk::adaptive_rps::LinearSearchStepControllerConfig config_;
  unsigned int current_rps_;
  bool latest_cycle_healthy_;
  bool reached_unhealthy_rps_;
};

// Factory that creates a LinearSearchStepController from a LinearSearchStepControllerConfig proto.
// Registered as an Envoy plugin.
class LinearSearchStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  StepControllerPtr createStepController(const Envoy::Protobuf::Message& config) override;
};

// A StepController that performs a binary search for the highest RPS that keeps metrics within
// thresholds. Note: This strategy is known to be problematic with some systems under test because
// it can send an overwhelming RPS that the system may not quickly recover from.
class BinarySearchStepController : public StepController {
public:
  explicit BinarySearchStepController(
      const nighthawk::adaptive_rps::BinarySearchStepControllerConfig& config);

  unsigned int GetCurrentRps() const override;
  bool IsConverged() const override;
  void UpdateAndRecompute(const nighthawk::adaptive_rps::BenchmarkResult& result) override;

private:
  const nighthawk::adaptive_rps::BinarySearchStepControllerConfig config_;
  unsigned int bottom_rps_;
  unsigned int top_rps_;
  unsigned int previous_rps_;
  unsigned int current_rps_;
};

// Factory that creates a BinarySearchStepController from a BinarySearchStepControllerConfig proto.
// Registered as an Envoy plugin.
class BinarySearchStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  StepControllerPtr createStepController(const Envoy::Protobuf::Message& config) override;
};

} // namespace AdaptiveRps
} // namespace Nighthawk