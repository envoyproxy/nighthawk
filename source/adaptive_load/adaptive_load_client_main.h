#pragma once

#include "envoy/common/time.h"

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {
namespace AdaptiveLoad {

class AdaptiveLoadMain : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  AdaptiveLoadMain(int argc, const char* const* argv, Envoy::TimeSource* time_source);
  uint32_t run();

private:
  std::string nighthawk_service_address_;
  std::string spec_filename_;
  std::string output_filename_;
  Envoy::TimeSource* time_source_;
};

} // namespace AdaptiveLoad
} // namespace Nighthawk
