#include "nighthawk/adaptive_load/metrics_plugin.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace Nighthawk {

absl::StatusOr<double> MetricsPlugin::GetMetricByNameWithReportingPeriod(
    [[maybe_unused]] absl::string_view metric_name,
    [[maybe_unused]] const ReportingPeriod& reporting_period) {
  return absl::Status{absl::StatusCode::kUnimplemented,
                      "GetMetricByNameWithReportingPeriod not implemented."};
}

} // namespace Nighthawk
