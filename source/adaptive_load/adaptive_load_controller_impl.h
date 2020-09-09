#include "envoy/common/time.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/adaptive_load/metrics_evaluator.h"
#include "nighthawk/adaptive_load/session_spec_proto_helper.h"
#include "nighthawk/adaptive_load/step_controller.h"
#include "nighthawk/common/nighthawk_service_client.h"

namespace Nighthawk {

class AdaptiveLoadControllerImpl : public AdaptiveLoadController {
public:
  /**
   * Constructs an implementation of the adaptive load controller main loop that relies on logic in
   * several helper objects. Through helpers, it performs Nighthawk Service benchmarks, obtains
   * metrics from MetricsPlugins, scores the results, and consults a StepController plugin to
   * determine the next load and detect convergence. All plugins are specified through the
   * AdaptiveLoadSessionSpec proto.
   *
   * Usage:
   *
   *   AdaptiveLoadControllerImpl controller(
   *       NighthawkServiceClientImpl(),
   *       MetricsEvaluatorImpl(),
   *       AdaptiveLoadSessionSpecProtoHelperImpl(),
   *       Envoy::Event::RealTimeSystem()); // NO_CHECK_FORMAT(real_time))
   *
   * @param nighthawk_service_client A helper that executes Nighthawk Service benchmarks given a
   * gRPC stub.
   * @param metrics_evaluator A helper that obtains metrics from MetricsPlugins and Nighthawk
   * Service responses, then scores them.
   * @param session_spec_proto_helper A helper that sets default values and performs validation in
   * an AdaptiveLoadSessionSpec proto.
   * @param time_source An abstraction of the system clock. Normally, just construct an
   * Envoy::Event::RealTimeSystem and pass it. NO_CHECK_FORMAT(real_time). If calling from an
   * Envoy-based process, there may be an existing TimeSource or TimeSystem to use. If calling
   * from a test, pass a fake TimeSource.
   */
  AdaptiveLoadControllerImpl(const NighthawkServiceClient& nighthawk_service_client,
                             const MetricsEvaluator& metrics_evaluator,
                             const AdaptiveLoadSessionSpecProtoHelper& session_spec_proto_helper,
                             Envoy::TimeSource& time_source);

  absl::StatusOr<nighthawk::adaptive_load::AdaptiveLoadSessionOutput> PerformAdaptiveLoadSession(
      nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
      const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) override;

private:
  /**
   * Gets the current load from the StepController, performs a benchmark via a Nighthawk Service,
   * and hands the result off for analysis.
   *
   * @param nighthawk_service_stub Nighthawk Service gRPC stub.
   * @param spec Proto describing the overall adaptive load session.
   * @param name_to_custom_plugin_map Common map from plugin names to MetricsPlugins loaded and
   * initialized once at the beginning of the session and passed to all calls of this function.
   * @param step_controller The active StepController specified in the session spec proto.
   * @param duration The duration of the benchmark.
   *
   * @return BenchmarkResult Proto containing either an error status or raw Nighthawk Service
   * results, metric values, and metric scores.
   */
  absl::StatusOr<nighthawk::adaptive_load::BenchmarkResult> PerformAndAnalyzeNighthawkBenchmark(
      nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
      const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
      const absl::flat_hash_map<std::string, MetricsPluginPtr>& name_to_custom_plugin_map,
      const StepController& step_controller, Envoy::ProtobufWkt::Duration duration);

  const NighthawkServiceClient& nighthawk_service_client_;
  const MetricsEvaluator& metrics_evaluator_;
  const AdaptiveLoadSessionSpecProtoHelper& session_spec_proto_helper_;
  Envoy::TimeSource& time_source_;
};

} // namespace Nighthawk
