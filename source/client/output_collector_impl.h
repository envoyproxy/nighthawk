#pragma once

#include <cstdint>

#include "envoy/common/time.h"

#include "nighthawk/client/output_collector.h"

#include "common/protobuf/protobuf.h"

namespace Nighthawk {
namespace Client {

class OutputCollectorImpl : public OutputCollector {
public:
  /**
   * @param time_source Time source that will be used to generate a timestamp in the output.
   * @param options The options that led up to the output that will be computed by this instance.
   */
  OutputCollectorImpl(Envoy::TimeSource& time_source, const Options& options);

  void addResult(absl::string_view name, const std::vector<StatisticPtr>& statistics,
                 const std::map<std::string, uint64_t>& counters) override;

  nighthawk::client::Output toProto() const override;

private:
  nighthawk::client::Output output_;
};

class ConsoleOutputCollectorImpl : public OutputCollectorImpl {
public:
  ConsoleOutputCollectorImpl(Envoy::TimeSource& time_source, const Options& options);
  std::string toString() const override;

private:
  std::string statIdtoFriendlyStatName(absl::string_view stat_id) const;
  std::string formatProtoDuration(const Envoy::Protobuf::Duration& duration) const;
};

class JsonOutputCollectorImpl : public OutputCollectorImpl {
public:
  JsonOutputCollectorImpl(Envoy::TimeSource& time_source, const Options& options);
  std::string toString() const override;
};

class YamlOutputCollectorImpl : public OutputCollectorImpl {
public:
  YamlOutputCollectorImpl(Envoy::TimeSource& time_source, const Options& options);
  std::string toString() const override;
};

} // namespace Client
} // namespace Nighthawk