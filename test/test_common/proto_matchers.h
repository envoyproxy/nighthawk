#pragma once

#include <string>

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

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

  bool equal = differ.Compare(arg, expected_proto);
  if (!equal) {
    *result_listener << "\n"
                     << "=======================Expected proto:===========================\n"
                     << absl::StrCat(expected_proto)
                     << "------------------is not equal to actual proto:------------------\n"
                     << absl::StrCat(arg)
                     << "------------------------the diff is:-----------------------------\n"
                     << diff
                     << "=================================================================\n";
  }
  return equal;
}

} // namespace Nighthawk
