#pragma once

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/event/real_time_system.h"

namespace Nighthawk {
namespace Client {

class OutputTransformMain : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  OutputTransformMain(int argc, const char* const* argv, std::istream& input);
  uint32_t run();

private:
  std::string readInput();
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
  std::string output_format_;
  std::istream& input_;
};

} // namespace Client
} // namespace Nighthawk
