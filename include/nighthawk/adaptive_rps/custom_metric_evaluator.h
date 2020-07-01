#pragma once

#include <memory>

#include "envoy/common/pure.h"
#include "envoy/registry/registry.h"
#include "envoy/config/typed_config.h"

// #include "external/envoy/source/common/config/utility.h"
// #include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_rps/custom_metric_evaluator.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

class CustomMetricEvaluator {
public:
  virtual ~CustomMetricEvaluator() = default;
  virtual double EvaluateMetric(double value) const PURE;
};

using CustomMetricEvaluatorPtr = std::unique_ptr<CustomMetricEvaluator>;

class CustomMetricEvaluatorConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~CustomMetricEvaluatorConfigFactory() override = default;
  std::string category() const override { return "nighthawk.custom_metric_evaluator"; }
  virtual CustomMetricEvaluatorPtr
  createCustomMetricEvaluator(const Envoy::Protobuf::Message& config_any) = 0;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
