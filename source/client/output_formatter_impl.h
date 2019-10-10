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
  void setProto(const nighthawk::client::Output& output) override { output_ = output; }
  static std::vector<std::string> getLowerCaseOutputFormats();

protected:
  nighthawk::client::Output output_;
};

class ConsoleOutputFormatterImpl : public OutputFormatterImpl {
public:
  using OutputFormatterImpl::OutputFormatterImpl;
  std::string toString() const override;
  static std::string statIdtoFriendlyStatName(absl::string_view stat_id);

private:
  std::string formatProtoDuration(const Envoy::ProtobufWkt::Duration& duration) const;
};

class JsonOutputFormatterImpl : public OutputFormatterImpl {
public:
  using OutputFormatterImpl::OutputFormatterImpl;
  std::string toString() const override;
};

class YamlOutputFormatterImpl : public OutputFormatterImpl {
public:
  using OutputFormatterImpl::OutputFormatterImpl;
  std::string toString() const override;
};

} // namespace Client
} // namespace Nighthawk