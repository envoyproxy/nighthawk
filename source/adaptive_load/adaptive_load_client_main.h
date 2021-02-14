#pragma once

#include "envoy/common/time.h"
#include "envoy/filesystem/filesystem.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

/**
 * Main implementation of the CLI wrapper around the adaptive load controller library.
 * Parses command line options, reads adaptive load session spec proto from a file,
 * runs an adaptive load session, and writes the output proto to a file.
 */
class AdaptiveLoadClientMain : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * Parses the command line arguments to class members.
   *
   * @param argc Standard argc passed through from the exe entry point.
   * @param argv Standard argv passed through from the exe entry point.
   * @param controller Adaptive load controller, passed in to allow unit testing of this class.
   * @param filesystem Abstraction of the filesystem, passed in to allow unit testing of this
   * class.
   *
   * @throw Nighthawk::Client::MalformedArgvException If command line constraints are violated.
   */
  AdaptiveLoadClientMain(int argc, const char* const* argv, AdaptiveLoadController& controller,
                         Envoy::Filesystem::Instance& filesystem);
  /**
   * Loads the adaptive load session spec proto from a file, runs an adaptive load session, and
   * writes the output proto to a file. File paths are taken from class members initialized in the
   * constructor.
   *
   * @return Exit code for this process.
   * @throw Nighthawk::NighthawkException If a file read or write error occurs.
   */
  uint32_t Run();
  /**
   * Describes the program inputs as parsed from the command line.
   */
  std::string DescribeInputs();

private:
  std::string nighthawk_service_address_;
  bool use_tls_;
  std::string spec_filename_;
  std::string output_filename_;
  AdaptiveLoadController& controller_;
  Envoy::Filesystem::Instance& filesystem_;
};

} // namespace Nighthawk
