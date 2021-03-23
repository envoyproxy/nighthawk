#pragma once

#include <cstdint>

#include "envoy/common/time.h"

#include "nighthawk/client/output_formatter.h"

#include "external/com_google_googletest/googletest/include/gtest/gtest_prod.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/output.pb.h"
#include "api/client/transform/fortio.pb.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace Nighthawk {
namespace Client {

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
  absl::StatusOr<std::string> formatProto(const nighthawk::client::Output& output) const override;
  static std::string statIdtoFriendlyStatName(absl::string_view stat_id);

private:
  std::string formatProtoDuration(const Envoy::ProtobufWkt::Duration& duration) const;
};

class JsonOutputFormatterImpl : public OutputFormatterImpl {
public:
  absl::StatusOr<std::string> formatProto(const nighthawk::client::Output& output) const override;
};

class YamlOutputFormatterImpl : public OutputFormatterImpl {
public:
  absl::StatusOr<std::string> formatProto(const nighthawk::client::Output& output) const override;
};

class DottedStringOutputFormatterImpl : public OutputFormatterImpl {
public:
  absl::StatusOr<std::string> formatProto(const nighthawk::client::Output& output) const override;
};

class FortioOutputFormatterImpl : public OutputFormatterImpl {
  FRIEND_TEST(FortioOutputCollectorTest, MissingGlobalResultGetGlobalResult);

public:
  absl::StatusOr<std::string> formatProto(const nighthawk::client::Output& output) const override;

protected:
  /**
   * Return the result that represents all workers (the one with the "global" name).
   *
   * @param output the Nighthawk output proto
   * @return the corresponding global result, or absl::Status if failed
   */
  absl::optional<const nighthawk::client::Result>
  getGlobalResult(const nighthawk::client::Output& output) const;

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
  absl::StatusOr<std::chrono::nanoseconds>
  getAverageExecutionDuration(const nighthawk::client::Output& output) const;

  /**
   * Converts a proto Duration to seconds
   * @param duration the proto Duration to convert
   * @return double the number of seconds
   */
  double durationToSeconds(const Envoy::ProtobufWkt::Duration& duration) const;
};

/**
 * Applies corrections to the output of the original FortioOutputFormatterImpl class,
 * to make the output adhere better to Fortio's actual output.
 * In particular, the proto json serializer outputs 64 bits integers as strings, whereas
 * Fortio outputs them unquoted / as integers, trusting that consumers side can take that
 * well. We also fix the RequestedQPS field which was defined as an integer, but gets
 * represented as a string in Fortio's json output.
 */
class FortioPedanticOutputFormatterImpl : public FortioOutputFormatterImpl {
public:
  /**
   * Format Nighthawk's native output proto to Fortio's output format.
   * This relies on the base class to provide the initial render, and applies
   * post processing to make corrections afterwards.
   *
   * @param output Nighthawk's native output proto that will be transformed.
   * @return std::string Fortio formatted json string.
   */
  absl::StatusOr<std::string> formatProto(const nighthawk::client::Output& output) const override;
};

} // namespace Client
} // namespace Nighthawk