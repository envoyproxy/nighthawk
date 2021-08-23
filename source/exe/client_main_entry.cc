#include "nighthawk/common/exception.h"

#include "source/client/client.h"

#include "absl/debugging/symbolize.h"

// NOLINT(namespace-nighthawk)

int main(int argc, char** argv) {
#ifndef __APPLE__
  // absl::Symbolize mostly works without this, but this improves corner case
  // handling, such as running in a chroot jail.
  absl::InitializeSymbolizer(argv[0]);
#endif
  std::unique_ptr<Nighthawk::Client::Main> client;

  try {
    client = std::make_unique<Nighthawk::Client::Main>(argc, argv);
  } catch (const Nighthawk::Client::NoServingException& e) {
    return EXIT_SUCCESS;
  } catch (const Nighthawk::Client::MalformedArgvException& e) {
    std::cerr << "Bad argument: " << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const Nighthawk::NighthawkException& e) {
    std::cerr << "An unknown error occurred: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return client->run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
