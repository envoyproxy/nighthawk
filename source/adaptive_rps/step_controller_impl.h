#pragma once

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/adaptive_rps/benchmark_result.pb.h"
#include "api/adaptive_rps/step_controller_impl.pb.h"
#include "nighthawk/adaptive_rps/step_controller.h"

namespace Nighthawk {
namespace AdaptiveRps {

class LinearSearchStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  StepControllerPtr createStepController(const Envoy::Protobuf::Message& config) override;
};

class LinearSearchStepController : public StepController {
public:
  explicit LinearSearchStepController(
      const nighthawk::adaptive_rps::LinearSearchStepControllerConfig& config);

  int GetCurrentRps() const override;
  bool IsConverged() const override;
  void UpdateAndRecompute(const nighthawk::adaptive_rps::BenchmarkResult& result) override;

private:
  const nighthawk::adaptive_rps::LinearSearchStepControllerConfig config_;
  int current_rps_;
  bool latest_cycle_healthy_;
  bool reached_unhealthy_rps_;
};

class BinarySearchStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  StepControllerPtr createStepController(const Envoy::Protobuf::Message& config) override;
};

class BinarySearchStepController : public StepController {
public:
  explicit BinarySearchStepController(
      const nighthawk::adaptive_rps::BinarySearchStepControllerConfig& config);

  int GetCurrentRps() const override;
  bool IsConverged() const override;
  void UpdateAndRecompute(const nighthawk::adaptive_rps::BenchmarkResult& result) override;

private:
  const nighthawk::adaptive_rps::BinarySearchStepControllerConfig config_;
  int bottom_rps_;
  int top_rps_;
  int previous_rps_;
  int current_rps_;
};

} // namespace AdaptiveRps
} // namespace Nighthawk