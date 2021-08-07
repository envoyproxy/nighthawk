#include "test/test_common/proto_matchers.h"

#include <string>

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "gtest/gtest.h"

namespace Nighthawk {

using ::Envoy::Protobuf::FieldDescriptor;
using ::Envoy::Protobuf::Message;
using ::Envoy::Protobuf::util::MessageDifferencer;

IgnoreUnknownFieldsGloballyByNumber::IgnoreUnknownFieldsGloballyByNumber(int ignored_field_number)
    : ignored_field_number_(ignored_field_number) {}

bool IgnoreUnknownFieldsGloballyByNumber::IsIgnored(
    const Message&, const Message&, const FieldDescriptor*,
    const std::vector<MessageDifferencer::SpecificField>&) {
  return false;
}

bool IgnoreUnknownFieldsGloballyByNumber::IsUnknownFieldIgnored(
    const Envoy::Protobuf::Message&, const Envoy::Protobuf::Message&,
    const Envoy::Protobuf::util::MessageDifferencer::SpecificField& field,
    const std::vector<MessageDifferencer::SpecificField>&) {
  return field.unknown_field_number == ignored_field_number_;
}

} // namespace Nighthawk
