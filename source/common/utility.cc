#include "source/common/utility.h"

#include <sys/socket.h>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/utility.h"

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace Nighthawk {

std::map<std::string, uint64_t>
Utility::mapCountersFromStore(const Envoy::Stats::Store& store,
                              const StoreCounterFilter& filter) const {
  std::map<std::string, uint64_t> results;

  for (const auto& stat : store.counters()) {
    if (filter(stat->name(), stat->value())) {
      std::string stat_name = stat->name();
      // Strip off cluster.[x]. & worker.[x]. prefixes.
      std::vector<std::string> v = absl::StrSplit(stat_name, '.');
      if (v[0] == "cluster" || v[0] == "worker") {
        v.erase(v.begin());
      }
      int tmp;
      if (absl::SimpleAtoi(v[0], &tmp)) {
        v.erase(v.begin());
      }
      stat_name = absl::StrJoin(v, ".");
      results[stat_name] += stat->value();
    }
  }
  return results;
}

size_t Utility::findPortSeparator(absl::string_view hostname) {
  if (hostname.size() > 0 && hostname[0] == '[') {
    return hostname.find(':', hostname.find(']'));
  }
  return hostname.rfind(':');
}

Envoy::Network::DnsLookupFamily
Utility::translateFamilyOptionString(nighthawk::client::AddressFamily::AddressFamilyOptions value) {
  switch (value) {
  case nighthawk::client::AddressFamily_AddressFamilyOptions_V4:
    return Envoy::Network::DnsLookupFamily::V4Only;
  case nighthawk::client::AddressFamily_AddressFamilyOptions_V6:
    return Envoy::Network::DnsLookupFamily::V6Only;
  case nighthawk::client::AddressFamily_AddressFamilyOptions_AUTO:
    return Envoy::Network::DnsLookupFamily::Auto;
  default:
    PANIC("not reached");
  }
}

void Utility::parseCommand(TCLAP::CmdLine& cmd, const int argc, const char* const* argv) {
  cmd.setExceptionHandling(false);
  try {
    cmd.parse(argc, argv);
  } catch (TCLAP::ArgException& e) {
    try {
      cmd.getOutput()->failure(cmd, e);
    } catch (const TCLAP::ExitException&) {
      // failure() has already written an informative message to stderr, so all that's left to do
      // is throw our own exception with the original message.
      throw Client::MalformedArgvException(e.what());
    }
  } catch (const TCLAP::ExitException& e) {
    // parse() throws an ExitException with status 0 after printing the output for --help and
    // --version.
    throw Client::NoServingException();
  }
}

bool Utility::parseHostPort(const std::string& host_port, std::string* address, int* port) {
  return RE2::FullMatch(host_port, R"((\d+\.\d+\.\d+\.\d+):(\d+))", address, port) ||
         RE2::FullMatch(host_port, R"((\[[.:0-9a-fA-F]+\]):(\d+))", address, port) ||
         RE2::FullMatch(host_port, R"(([-.0-9a-zA-Z]+):(\d+))", address, port);
}

// Obtains an available TCP or UDP port. Throws an exception if one cannot be
// allocated.
uint16_t Utility::GetAvailablePort(bool udp, nighthawk::client::AddressFamily::AddressFamilyOptions address_family) {
  int family = (address_family == nighthawk::client::AddressFamily::V4) ? AF_INET : AF_INET6;
  int sock = socket(family, udp ? SOCK_DGRAM : SOCK_STREAM, udp ? 0 : IPPROTO_TCP);
  if (sock < 0) {
    throw NighthawkException(absl::StrCat("could not create socket: ", Envoy::errorDetails(errno)));
    return 0;
  }

  // Reuseaddr lets us start up a server immediately after it exits
  int one = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &one, sizeof(one)) < 0) {
    throw NighthawkException(absl::StrCat("setsockopt: ", Envoy::errorDetails(errno)));
    close(sock);
    return 0;
  }
  union {
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } addr;
  size_t size;
  if (family == AF_INET) {
    size = sizeof(sockaddr_in);
    memset(&addr, 0, size);
    addr.sin.sin_family = AF_INET;
    addr.sin.sin_addr.s_addr = INADDR_ANY;
    addr.sin.sin_port = 0;
  } else {
    size = sizeof(sockaddr_in6);
    memset(&addr, 0, size);
    addr.sin6.sin6_family = AF_INET6;
    addr.sin6.sin6_addr = in6addr_any;
    addr.sin6.sin6_port = 0;
  }

  if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), size) < 0) {
    if (errno == EADDRINUSE) {
      throw NighthawkException(absl::StrCat("Port allocated already in use"));
    } else {
      throw NighthawkException(
          absl::StrCat("Could not bind to process: ", Envoy::errorDetails(errno)));
    }
    return 0;
  }

  socklen_t len = size;
  if (getsockname(sock, reinterpret_cast<struct sockaddr*>(&addr), &len) == -1) {
    throw NighthawkException(absl::StrCat("Could not get sock name: ", Envoy::errorDetails(errno)));
    return 0;
  }

  uint16_t port =
      ntohs(family == AF_INET ? reinterpret_cast<struct sockaddr_in*>(&addr)->sin_port
                              : reinterpret_cast<struct sockaddr_in6*>(&addr)->sin6_port);

  // close the socket, freeing the port to be used later.
  if (close(sock) < 0) {
    throw NighthawkException(absl::StrCat("Could not close socket: ", Envoy::errorDetails(errno)));
    return 0;
  }

  return port;
}

} // namespace Nighthawk
