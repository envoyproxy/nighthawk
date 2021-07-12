#include <chrono>

#include "external/envoy/test/test_common/utility.h"

#include "source/common/request_source_impl.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace Client {

class RequestSourceTest : public testing::Test {};

TEST_F(RequestSourceTest, StaticRequestSourceImpl) {
  Envoy::Http::RequestHeaderMapPtr header{new Envoy::Http::TestRequestHeaderMapImpl()};
  Envoy::Http::RequestHeaderMap* unsafe_header_ptr = header.get();
  uint32_t yields = 5;
  StaticRequestSourceImpl impl(std::move(header), yields);
  auto generator = impl.get();
  while (yields--) {
    ASSERT_EQ(generator()->header().get(), unsafe_header_ptr);
  }
  ASSERT_EQ(generator(), nullptr);
}

} // namespace Client
} // namespace Nighthawk
