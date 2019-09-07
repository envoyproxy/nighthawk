#include <chrono>

#include "external/envoy/test/test_common/utility.h"

#include "common/header_source_impl.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace Client {

class HeaderSourceTest : public testing::Test {};

TEST_F(HeaderSourceTest, StaticHeaderSourceImpl) {
  Envoy::Http::HeaderMapPtr header{new Envoy::Http::TestHeaderMapImpl()};
  Envoy::Http::HeaderMap* unsafe_header_ptr = header.get();
  uint32_t yields = 5;
  StaticHeaderSourceImpl impl(std::move(header), yields);
  auto generator = impl.get();
  while (yields--) {
    ASSERT_EQ(generator().get(), unsafe_header_ptr);
  }
  ASSERT_EQ(generator(), nullptr);
}

} // namespace Client
} // namespace Nighthawk
