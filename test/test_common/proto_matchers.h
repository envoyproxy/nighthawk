#include <string>

#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/source/common/protobuf/well_known.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

// A custom IgnoreCriteria that can be added to a MessageDifferencer to ignore
// unknown fields by their field number, regardless of where they appear in a
// message.
class IgnoreUnknownFieldsGloballyByNumber
    : public Envoy::Protobuf::util::MessageDifferencer::IgnoreCriteria {
public:
  // Constructs an ignore criteria instance that will ignore differences in all
  // unknown proto fields whose field number matches the one specified.
  explicit IgnoreUnknownFieldsGloballyByNumber(int ignored_field_number);

  // Does not ignore any fields on this implementation.
  // Implemented only to satisfy the interface.
  bool IsIgnored(const Envoy::Protobuf::Message& message1, const Envoy::Protobuf::Message& message2,
                 const Envoy::Protobuf::FieldDescriptor* field,
                 const std::vector<Envoy::Protobuf::util::MessageDifferencer::SpecificField>&
                     parent_fields) override;

  // Ignores an unknown field if its field number equals to the one provided to the
  // constructor.
  bool IsUnknownFieldIgnored(
      const Envoy::Protobuf::Message& message1, const Envoy::Protobuf::Message& message2,
      const Envoy::Protobuf::util::MessageDifferencer::SpecificField& field,
      const std::vector<Envoy::Protobuf::util::MessageDifferencer::SpecificField>& parent_fields)
      override;

private:
  const int ignored_field_number_;
};

// Compares two proto messages for equality.
// Prints diff on failures.
//
// Example use:
//   proto2::Message actual_proto;
//   proto2::Message expected_proto;
//
//   EXPECT_THAT(actual_proto, EqualsProto(expected_proto));
MATCHER_P(EqualsProto, expected_proto, "is equal to the expected_proto") {
  std::string diff;
  Envoy::Protobuf::util::MessageDifferencer differ;
  differ.ReportDifferencesToString(&diff);

  // Envoy proto messages have a well known field with this number that needs to
  // be ignored in proto comparisons.
  differ.AddIgnoreCriteria(
      new IgnoreUnknownFieldsGloballyByNumber(Envoy::ProtobufWellKnown::OriginalTypeFieldNumber));

  bool equal = differ.Compare(arg, expected_proto);
  if (!equal) {
    *result_listener << "\n"
                     << "=======================Expected proto:===========================\n"
                     << expected_proto.DebugString()
                     << "------------------is not equal to actual proto:------------------\n"
                     << arg.DebugString()
                     << "------------------------the diff is:-----------------------------\n"
                     << diff
                     << "=================================================================\n";
  }
  return equal;
}

} // namespace Nighthawk
