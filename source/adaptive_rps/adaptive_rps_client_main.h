#pragma once

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {
namespace AdaptiveRps {

class AdaptiveRpsMain : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  AdaptiveRpsMain(int argc, const char* const* argv);
  uint32_t run();

private:
  std::string nighthawk_service_address_;
  std::string spec_filename_;
  std::string output_filename_;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
