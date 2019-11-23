#include "common/request_stream_grpc_client_impl.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

// The grpc client itself is tested via the python based integration tests.
// It is convenient to test message translation here.
class UtilityTest : public Test {};

// TODO(oschaaf): thoroughly test this.
TEST_F(UtilityTest, MessageTranslationHelper) {
  nighthawk::client::RequestStreamResponse response;
  Envoy::Http::HeaderMapImpl header;
  auto request = ProtoRequestHelper::messageToRequest(header, response);
}

} // namespace Nighthawk
