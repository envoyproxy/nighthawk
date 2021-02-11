// Command line adaptive load controller driving a Nighthawk Service.
#include <iostream>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/exe/platform_impl.h"

#include "common/nighthawk_service_client_impl.h"

#include "absl/debugging/symbolize.h"
#include "adaptive_load/adaptive_load_client_main.h"
#include "adaptive_load/adaptive_load_controller_impl.h"
#include "adaptive_load/metrics_evaluator_impl.h"
#include "adaptive_load/session_spec_proto_helper_impl.h"

// NOLINT(namespace-nighthawk)

int main(int argc, char* argv[]) {
#ifndef __APPLE__
  // absl::Symbolize mostly works without this, but this improves corner case
  // handling, such as running in a chroot jail.
  absl::InitializeSymbolizer(argv[0]);
#endif
  Nighthawk::NighthawkServiceClientImpl nighthawk_service_client;
  Nighthawk::MetricsEvaluatorImpl metrics_evaluator;
  Nighthawk::AdaptiveLoadSessionSpecProtoHelperImpl spec_proto_helper;
  Envoy::Event::RealTimeSystem time_system; // NO_CHECK_FORMAT(real_time)
  Nighthawk::AdaptiveLoadControllerImpl controller(nighthawk_service_client, metrics_evaluator,
                                                   spec_proto_helper, time_system);
  Envoy::PlatformImpl platform_impl;
  try {
    Nighthawk::AdaptiveLoadClientMain program(argc, argv, controller, platform_impl.fileSystem());
    return program.Run();
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
