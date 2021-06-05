#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/common/pure.h"

#include "api/client/output.pb.validate.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace Nighthawk {
namespace Client {

/**
 * Transforms from nighthawk::client::Output to an output format.
 */
class OutputFormatter {
public:
  virtual ~OutputFormatter() = default;

  /**
   * @return absl::StatusOr<std::string> serialized representation of output, or an error.
   * The specific format depends on the derived class, for example human-readable or json.
   */
  virtual absl::StatusOr<std::string>
  formatProto(const nighthawk::client::Output& output) const PURE;
};

using OutputFormatterPtr = std::unique_ptr<OutputFormatter>;

} // namespace Client
} // namespace Nighthawk
