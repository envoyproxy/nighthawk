#pragma once

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {
namespace AdaptiveRps {

class AdaptiveRpsMain : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  AdaptiveRpsMain(int argc, const char* const* argv);
  uint32_t run();

private:
  std::string api_server_;
  std::stringspec_filename_;
  std::stringoutput_filename_;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
