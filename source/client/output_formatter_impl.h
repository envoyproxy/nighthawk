#pragma once

#include <cstdint>

#include "envoy/common/time.h"

#include "nighthawk/client/output_formatter.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/output.pb.h"
#include "api/client/transform/fortio.pb.h"

#include "absl/strings/string_view.h"

namespace Nighthawk {

class OutputFormatterImpl : public OutputFormatter {
public:
  static std::vector<std::string> getLowerCaseOutputFormats();

protected:
  void iteratePercentiles(
      const nighthawk::client::Statistic& statistic,
      const std::function<void(const nighthawk::client::Percentile&)>& callback) const;
};

class ConsoleOutputFormatterImpl : public OutputFormatterImpl {
public:
  std::string formatProto(const nighthawk::client::Output& output) const override;
  static std::string statIdtoFriendlyStatName(absl::string_view stat_id);

private:
  std::string formatProtoDuration(const Envoy::ProtobufWkt::Duration& duration) const;
};

class JsonOutputFormatterImpl : public OutputFormatterImpl {
public:
  std::string formatProto(const nighthawk::client::Output& output) const override;
};

class YamlOutputFormatterImpl : public OutputFormatterImpl {
public:
  std::string formatProto(const nighthawk::client::Output& output) const override;
};

class DottedStringOutputFormatterImpl : public OutputFormatterImpl {
public:
  std::string formatProto(const nighthawk::client::Output& output) const override;
};

class FortioOutputFormatterImpl : public OutputFormatterImpl {
public:
  std::string formatProto(const nighthawk::client::Output& output) const override;

protected:
  /**
   * Return the result that represents all workers (the one with the "global" name).
   *
   * @param output the Nighthawk output proto
   * @return the corresponding global result
   * @throws NighthawkException if global result is not found
   */
  const nighthawk::client::Result& getGlobalResult(const nighthawk::client::Output& output) const;

  /**
   * Return the counter with the specified name.
   *
   * @param result a single Nighthawk result, preferably the global result
   * @param counter_name the name of the counter to return
   * @param value_if_not_found value that will be returned when the counter does not exist in the
   * output.
   * @return True iff a counter was found.
   */
  uint64_t getCounterValue(const nighthawk::client::Result& result, absl::string_view counter_name,
                           const uint64_t value_if_not_found = 0) const;

  /**
   * Return the statistic that represents the request/response round-trip times.
   *
   * @param result a single Nighthawk result, preferably the global result
   * @param stat_id the id of the statistic that we are looking for
   * @return a pointer to the corresponding request/response statistic, or
   * nullptr if no statistic with the request id was found
   */
  const nighthawk::client::Statistic* findStatistic(const nighthawk::client::Result& result,
                                                    absl::string_view stat_id) const;

  const nighthawk::client::DurationHistogram
  renderFortioDurationHistogram(const nighthawk::client::Statistic& statistic) const;

  /**
   * Gets the average execution duration based on averaging all worker sequencer execution
   * durations.
   *
   * @param output the Nighthawk output proto
   * @return the corresponding average execution duration in nanoseconds
   */
  std::chrono::nanoseconds
  getAverageExecutionDuration(const nighthawk::client::Output& output) const;

  /**
   * Converts a proto Duration to seconds
   * @param duration the proto Duration to convert
   * @return double the number of seconds
   */
  double durationToSeconds(const Envoy::ProtobufWkt::Duration& duration) const;
};

 } // namespace Nighthawk