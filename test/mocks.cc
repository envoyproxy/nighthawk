#include <chrono>
#include <memory>

#include "gmock/gmock.h"

#include "test/mocks.h"

using namespace std::chrono_literals;

namespace Nighthawk {

MockPlatformUtil::MockPlatformUtil() = default;
MockPlatformUtil::~MockPlatformUtil() = default;

MockRateLimiter::MockRateLimiter() = default;
MockRateLimiter::~MockRateLimiter() = default;

FakeSequencerTarget::FakeSequencerTarget() = default;
FakeSequencerTarget::~FakeSequencerTarget() = default;

MockSequencerTarget::MockSequencerTarget() = default;
MockSequencerTarget::~MockSequencerTarget() = default;

MockSequencer::MockSequencer() = default;
MockSequencer::~MockSequencer() = default;

MockOptions::MockOptions() = default;
MockOptions::~MockOptions() = default;

MockBenchmarkClientFactory::MockBenchmarkClientFactory() = default;
MockBenchmarkClientFactory::~MockBenchmarkClientFactory() = default;

MockSequencerFactory::MockSequencerFactory() = default;
MockSequencerFactory::~MockSequencerFactory() = default;

MockStoreFactory::MockStoreFactory() = default;
MockStoreFactory::~MockStoreFactory() = default;

MockStatisticFactory::MockStatisticFactory() = default;
MockStatisticFactory::~MockStatisticFactory() = default;

MockBenchmarkClient::MockBenchmarkClient() = default;
MockBenchmarkClient::~MockBenchmarkClient() = default;

} // namespace Nighthawk