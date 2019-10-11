#pragma once

#include <cstdint>

#include "envoy/common/time.h"

#include "nighthawk/client/output_formatter.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

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

} // namespace Client
} // namespace Nighthawk