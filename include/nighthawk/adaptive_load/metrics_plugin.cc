#include "nighthawk/adaptive_load/metrics_plugin.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace Nighthawk {

absl::StatusOr<double> MetricsPlugin::GetMetricByNameWithTime(
    [[maybe_unused]] absl::string_view metric_name,
    [[maybe_unused]] const google::protobuf::Timestamp& start_time,
    [[maybe_unused]] const google::protobuf::Duration& duration) {
  return absl::Status{absl::StatusCode::kUnimplemented, "GetMetricByNameWithTime not implemented."};
}

} // namespace Nighthawk
