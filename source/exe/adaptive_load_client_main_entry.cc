// Command line adaptive RPS controller driving a Nighthawk Service.
#include <iostream>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/exe/platform_impl.h"

#include "absl/debugging/symbolize.h"
#include "adaptive_load/adaptive_load_client_main.h"

// NOLINT(namespace-nighthawk)

int main(int argc, char* argv[]) {
#ifndef __APPLE__
  // absl::Symbolize mostly works without this, but this improves corner case
  // handling, such as running in a chroot jail.
  absl::InitializeSymbolizer(argv[0]);
#endif
  Envoy::Event::RealTimeSystem time_system; // NO_CHECK_FORMAT(real_time)
  Envoy::PlatformImpl platform_impl;
  try {
    Nighthawk::AdaptiveLoadClientMain program(argc, argv, platform_impl.fileSystem(),
                                              time_system); // NOLINT
    return program.run();
  } catch (const Nighthawk::Client::NoServingException& e) {
    return EXIT_SUCCESS;
  } catch (const Nighthawk::Client::MalformedArgvException& e) {
    std::cerr << "Invalid args: " << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const Nighthawk::NighthawkException& e) {
    std::cerr << "Failure: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
