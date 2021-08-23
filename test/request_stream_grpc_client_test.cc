#include "envoy/api/v2/core/base.pb.h"
#include "envoy/config/core/v3/base.pb.h"

#include "external/envoy/test/test_common/utility.h"

#include "api/request_source/service.pb.h"

#include "source/common/request_impl.h"
#include "source/common/request_stream_grpc_client_impl.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace {

using ::nighthawk::request_source::RequestSpecifier;

// The grpc client itself is tested via the python based integration tests.
// It is convenient to test message translation here.
class ProtoRequestHelperTest : public Test {
public:
  void translateExpectingEqual() {
    auto request = ProtoRequestHelper::messageToRequest(base_header_, response_);
    // We test for equality. If we observe mismatch, we use EXPECT_EQ which is guaranteed
    // to fail -- but will provide much more helpful output.
    if (!Envoy::TestUtility::headerMapEqualIgnoreOrder(expected_header_, *request->header())) {
      EXPECT_EQ(expected_header_, *request->header()) << "expected headers:\n"
                                                      << expected_header_ << "\nactual headers:\n"
                                                      << *request->header() << "\n";
    };
  }

protected:
  nighthawk::request_source::RequestStreamResponse response_;
  Envoy::Http::TestRequestHeaderMapImpl base_header_;
  Envoy::Http::TestRequestHeaderMapImpl expected_header_;
};

TEST_F(ProtoRequestHelperTest, EmptyRequestSpecifier) { translateExpectingEqual(); }

// Test all explicit headers we offer in the proto api.
TEST_F(ProtoRequestHelperTest, ExplicitFields) {
  auto* request_specifier = response_.mutable_request_specifier();
  request_specifier->mutable_authority()->set_value("foohost");
  request_specifier->mutable_path()->set_value("/");
  request_specifier->mutable_method()->set_value("GET");
  request_specifier->mutable_content_length()->set_value(999);
  expected_header_ = Envoy::Http::TestRequestHeaderMapImpl{
      {":method", "GET"}, {"content-length", "999"}, {":path", "/"}, {":authority", "foohost"}};
  translateExpectingEqual();
}

// Test the generic header api we offer in the proto api using Envoy API v2
// primitives.
TEST_F(ProtoRequestHelperTest, GenericHeaderFieldsUsingDeprecatedEnvoyV2Api) {
  RequestSpecifier* request_specifier = response_.mutable_request_specifier();
  envoy::api::v2::core::HeaderMap* headers = request_specifier->mutable_headers();
  envoy::api::v2::core::HeaderValue* header_1 = headers->add_headers();
  header_1->set_key("header1");
  header_1->set_value("value1");
  envoy::api::v2::core::HeaderValue* header_2 = headers->add_headers();
  header_2->set_key("header2");
  header_2->set_value("value2");
  // We re-add the same header, but do not expect that to show up in the translation because we
  // always replace.
  headers->add_headers()->MergeFrom(*header_2);
  expected_header_ =
      Envoy::Http::TestRequestHeaderMapImpl{{"header1", "value1"}, {"header2", "value2"}};
  translateExpectingEqual();
}

// Test the generic header api we offer in the proto api using Envoy API v3
// primitives.
TEST_F(ProtoRequestHelperTest, GenericHeaderFieldsUsingEnvoyV3Api) {
  RequestSpecifier* request_specifier = response_.mutable_request_specifier();
  envoy::config::core::v3::HeaderMap* headers = request_specifier->mutable_v3_headers();
  envoy::config::core::v3::HeaderValue* header_1 = headers->add_headers();
  header_1->set_key("header1");
  header_1->set_value("value1");
  envoy::config::core::v3::HeaderValue* header_2 = headers->add_headers();
  header_2->set_key("header2");
  header_2->set_value("value2");
  // We re-add the same header, but do not expect that to show up in the translation because we
  // always replace.
  headers->add_headers()->MergeFrom(*header_2);
  expected_header_ =
      Envoy::Http::TestRequestHeaderMapImpl{{"header1", "value1"}, {"header2", "value2"}};
  translateExpectingEqual();
}

// Test ambiguous host configuration behavior yields expected results.
TEST_F(ProtoRequestHelperTest, AmbiguousHost) {
  auto* request_specifier = response_.mutable_request_specifier();
  request_specifier->mutable_authority()->set_value("foohost");
  expected_header_ = Envoy::Http::TestRequestHeaderMapImpl{{":authority", "foohost"}};
  // We also set the host via the headers. The explicit field we use above for that
  // should prevail.
  auto* headers = request_specifier->mutable_headers();
  auto* header_1 = headers->add_headers();
  header_1->set_key("host");
  header_1->set_value("foohost2");
  translateExpectingEqual();
}

} // namespace
} // namespace Nighthawk
