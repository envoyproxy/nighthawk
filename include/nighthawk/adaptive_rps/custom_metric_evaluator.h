#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "api/adaptive_rps/custom_metric_evaluator.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

// An interface for custom functions that measure a metric relative to a threshold.
class CustomMetricEvaluator {
public:
  virtual ~CustomMetricEvaluator() = default;
  // Returns a value between -1.0 and 1.0: 1.0 means the metric value is highly favorable and a
  // large RPS increase should be attempted. -1.0 means the metric value is highly unfavorable and a
  // large RPS decrease is needed. 0.0 means the metric is exactly at the threshold.
  virtual double EvaluateMetric(double value) const PURE;
};

using CustomMetricEvaluatorPtr = std::unique_ptr<CustomMetricEvaluator>;

// A factory that must be implemented for each CustomMetricEvaluator plugin. It instantiates the
// specific CustomMetricEvaluator class after unpacking the plugin-specific config proto.
class CustomMetricEvaluatorConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~CustomMetricEvaluatorConfigFactory() override = default;
  std::string category() const override { return "nighthawk.custom_metric_evaluator"; }
  // Instantiates the specific CustomMetricEvaluator class. Casts |message| to Any, unpacks it to
  // the plugin-specific proto, and passes the strongly typed proto to the constructor.
  virtual CustomMetricEvaluatorPtr
  createCustomMetricEvaluator(const Envoy::Protobuf::Message& config_any) = 0;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
