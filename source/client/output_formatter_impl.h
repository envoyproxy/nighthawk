#pragma once

#include <cstdint>

#include "envoy/common/time.h"

#include "nighthawk/client/output_formatter.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/output.pb.h"

#include "absl/strings/string_view.h"

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
   * @return the corresponding counter
   * @throws NighthawkException if counter with given name is not found
   */
  const nighthawk::client::Counter& getCounterByName(const nighthawk::client::Result& result,
                                                     absl::string_view counter_name) const;

  /**
   * Return the statistic that represents the request/response round-trip times.
   *
   * @param result a single Nighthawk result, preferably the global result
   * @return the corresponding request/response statistic
   * @throws NighthawkException if request/response statistic is not found
   */
  const nighthawk::client::Statistic&
  getRequestResponseStatistic(const nighthawk::client::Result& result) const;
};

} // namespace Client
} // namespace Nighthawk