#include "nighthawk/adaptive_load/metrics_plugin.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace Nighthawk {

absl::StatusOr<double>
MetricsPlugin::GetMetricByNameWithTime(absl::string_view metric_name,
                                       const google::protobuf::Timestamp& start_time,
                                       const google::protobuf::Duration& duration) {
  return absl::Status{absl::StatusCode::kUnimplemented, "GetMetricByNameWithTime not implemented."};
}

} // namespace Nighthawk
