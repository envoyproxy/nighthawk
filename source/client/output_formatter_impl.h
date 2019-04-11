#pragma once

#include "nighthawk/client/output_formatter.h"

#include "envoy/common/time.h"

#include "common/protobuf/protobuf.h"

#include <cstdint>

namespace Nighthawk {
namespace Client {

class OutputFormatterImpl : public OutputFormatter {
public:
  /**
   * @param time_source Time source that will be used to generate a timestamp in the output.
   * @param options The options that led up to the output that will be computed by this instance.
   */
  OutputFormatterImpl(Envoy::TimeSource& time_source, const Options& options);

  void addResult(absl::string_view name, const std::vector<StatisticPtr>& statistics,
                 const std::map<std::string, uint64_t>& counters) override;

  nighthawk::client::Output toProto() const override;

private:
  nighthawk::client::Output output_;
};

class ConsoleOutputFormatterImpl : public OutputFormatterImpl {
public:
  ConsoleOutputFormatterImpl(Envoy::TimeSource& time_source, const Options& options);
  std::string toString() const override;
};

class JsonOutputFormatterImpl : public OutputFormatterImpl {
public:
  JsonOutputFormatterImpl(Envoy::TimeSource& time_source, const Options& options);
  std::string toString() const override;
};

class YamlOutputFormatterImpl : public OutputFormatterImpl {
public:
  YamlOutputFormatterImpl(Envoy::TimeSource& time_source, const Options& options);
  std::string toString() const override;
};

} // namespace Client
} // namespace Nighthawk