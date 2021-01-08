#include <csignal>
#include <future>

#include "common/signal_handler.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

TEST(SignalHandlerTest, SignalGetsHandled) {
  for (const auto signal : {SIGTERM, SIGINT}) {
    bool signal_handled = false;
    std::promise<void> signal_all_threads_running;

    SignalHandler signal_handler([&signal_handled, &signal_all_threads_running]() {
      signal_handled = true;
      signal_all_threads_running.set_value();
    });
    std::raise(signal);
    signal_all_threads_running.get_future().wait();
    EXPECT_TRUE(signal_handled);
  }
}

TEST(SignalHandlerTest, DestructDoesNotFireHandler) {
  bool signal_handled = false;
  {
    SignalHandler signal_handler([&signal_handled]() { signal_handled = true; });
  }
  EXPECT_FALSE(signal_handled);
}

} // namespace
} // namespace Nighthawk
