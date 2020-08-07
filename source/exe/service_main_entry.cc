#include "nighthawk/common/exception.h"

#include "client/service_main.h"

#include "absl/debugging/symbolize.h"

// NOLINT(namespace-nighthawk)

int main(int argc, const char** argv) {
#ifndef __APPLE__
  // absl::Symbolize mostly works without this, but this improves corner case
  // handling, such as running in a chroot jail.
  absl::InitializeSymbolizer(argv[0]);
#endif
  try {
    Nighthawk::ServiceMain service(argc, argv); // NOLINT
    service.start();
    service.wait();
  } catch (const Nighthawk::NoServingException& e) {
    return EXIT_SUCCESS;
  } catch (const Nighthawk::MalformedArgvException& e) {
    return EXIT_FAILURE;
  } catch (const Nighthawk::NighthawkException& e) {
    std::cerr << "Failure: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
