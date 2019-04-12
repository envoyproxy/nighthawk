#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "nighthawk/client/options.h"
#include "nighthawk/common/exception.h"

#include "nighthawk/source/client/options.pb.h"

namespace Nighthawk {
namespace Client {

class OptionsImpl : public Options {
public:
  OptionsImpl(int argc, const char* const* argv);

  Client::CommandLineOptionsPtr toCommandLineOptions() const override;

  uint64_t requestsPerSecond() const override { return requests_per_second_; }
  uint64_t connections() const override { return connections_; }
  std::chrono::seconds duration() const override { return std::chrono::seconds(duration_); }
  std::chrono::seconds timeout() const override { return std::chrono::seconds(timeout_); }
  std::string uri() const override { return uri_; }
  bool h2() const override { return h2_; }
  std::string concurrency() const override { return concurrency_; }
  std::string verbosity() const override { return verbosity_; };
  std::string outputFormat() const override { return output_format_; };

private:
  uint64_t requests_per_second_;
  uint64_t connections_;
  uint64_t duration_;
  uint64_t timeout_;
  std::string uri_;
  bool h2_;
  std::string concurrency_;
  std::string verbosity_;
  std::string output_format_;
};

} // namespace Client
} // namespace Nighthawk
