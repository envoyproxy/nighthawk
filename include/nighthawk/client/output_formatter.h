#pragma once

#include <memory>

#include "envoy/common/pure.h"

#include "api/client/output.pb.validate.h"

namespace Nighthawk {
namespace Client {

/**
 * Transforms from nighthawk::client::Output to an output format.
 */
class OutputFormatter {
public:
  virtual ~OutputFormatter() = default;
  virtual void setProto(const nighthawk::client::Output& output) PURE;
  virtual std::string toString() const PURE;
};

using OutputFormatterPtr = std::unique_ptr<OutputFormatter>;

} // namespace Client
} // namespace Nighthawk
