#include <iostream>

#include "nighthawk/common/exception.h"

#include "source/client/output_transform_main.h"

#include "absl/debugging/symbolize.h"

// NOLINT(namespace-nighthawk)

int main(int argc, char** argv) {

#ifndef __APPLE__
  // absl::Symbolize mostly works without this, but this improves corner case
  // handling, such as running in a chroot jail.
  absl::InitializeSymbolizer(argv[0]);
#endif
  try {
    Nighthawk::Client::OutputTransformMain program(argc, argv, std::cin); // NOLINT
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
