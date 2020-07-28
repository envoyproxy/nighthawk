#include "nighthawk/common/factories.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRequestSourceConstructor : public RequestSourceConstructorInterface {
public:
  MockRequestSourceConstructor();
  MOCK_METHOD(RequestSourcePtr, createStaticRequestSource,
              (Envoy::Http::RequestHeaderMapPtr&&, const uint64_t max_yields), (const, override));
  MOCK_METHOD(RequestSourcePtr, createRemoteRequestSource,
              (Envoy::Http::RequestHeaderMapPtr && base_header, uint32_t header_buffer_length),
              (const, override));
};

} // namespace Nighthawk