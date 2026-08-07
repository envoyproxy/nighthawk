#include <atomic>
#include <chrono>
#include <csignal>
#include <future>
#include <signal.h>
#include <thread>

#include "source/common/signal_handler.h"

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

// Regression test: ~SignalHandler() used to leave the file-scope static `signal_handler`
// installed for SIGTERM/SIGINT, whose delegate (the file-scope std::function
// `signal_handler_delegate`) captured a `this` pointer to the DESTROYED SignalHandler
// instance - a dangling delegate; any later signal was undefined behavior, best-case a
// silent no-op. The destructor now restores the dispositions saved at construction time
// (and clears the delegate), so after destruction SIGTERM/SIGINT are back to SIG_DFL.
TEST(SignalHandlerTest, DestructorRestoresDefaultSignalDisposition) {
  // Tests in this binary share a process, and earlier tests may already have left the
  // stale nighthawk handler installed for these signals. To be independent of test
  // ordering, force a known clean state (SIG_DFL) before constructing the handler under
  // test. We intentionally do NOT reinstall the captured previous handlers afterwards:
  // they may be the stale (dangling-delegate) handler, and SIG_DFL is the safe,
  // non-polluting end state - every other test installs its own SignalHandler before
  // raising signals.
  const auto previous_term_disposition = std::signal(SIGTERM, SIG_DFL);
  const auto previous_int_disposition = std::signal(SIGINT, SIG_DFL);
  ASSERT_NE(previous_term_disposition, SIG_ERR);
  ASSERT_NE(previous_int_disposition, SIG_ERR);

  {
    SignalHandler signal_handler([]() {});
  }

  // Query the current dispositions via a std::signal() round-trip: std::signal returns
  // the handler that was installed at the time of the call.
  const auto post_destruction_term = std::signal(SIGTERM, SIG_DFL);
  const auto post_destruction_int = std::signal(SIGINT, SIG_DFL);
  EXPECT_EQ(post_destruction_term, SIG_DFL)
      << "~SignalHandler() left a stale SIGTERM handler installed";
  EXPECT_EQ(post_destruction_int, SIG_DFL)
      << "~SignalHandler() left a stale SIGINT handler installed";
}

// Second-signal escalation: after the FIRST signal is handled (the shutdown thread has
// consumed it and run the callback), the SignalHandler restores the signal dispositions
// that were active before it was constructed. A SECOND Ctrl-C therefore gets the OS
// default action - process termination - instead of being silently swallowed, giving a
// user stuck in a hanging shutdown/drain a force-quit escalation path.
//
// We deliberately do NOT raise a real second SIGTERM here: with the escalation in place
// it would kill the test binary. Instead we prove the escalation by observing that the
// installed disposition falls back to SIG_DFL. The restore runs on the shutdown thread
// after the callback returns, so it is asynchronous with respect to the callback firing;
// we poll for it with a bounded deadline. The poll uses sigaction() with a null
// new-action, which is a pure READ of the current disposition - unlike a
// std::signal(SIGTERM, <probe>) round-trip, it cannot race with (and clobber) the
// shutdown thread's restore.
TEST(SignalHandlerTest, SecondSignalFallsBackToDefaultDisposition) {
  // Tests in this binary share a process; force a known clean state so the dispositions
  // the SignalHandler under test saves (and later restores) are SIG_DFL, independent of
  // test ordering.
  ASSERT_NE(std::signal(SIGTERM, SIG_DFL), SIG_ERR);
  ASSERT_NE(std::signal(SIGINT, SIG_DFL), SIG_ERR);

  std::atomic<int> callback_count{0};
  std::promise<void> first_callback_fired;

  SignalHandler signal_handler([&callback_count, &first_callback_fired]() {
    if (++callback_count == 1) {
      first_callback_fired.set_value();
    }
  });

  std::raise(SIGTERM);
  first_callback_fired.get_future().wait();
  EXPECT_EQ(callback_count, 1);

  // Poll (read-only) until both dispositions are back to SIG_DFL, bounded at 2 seconds.
  const auto dispositions_restored = []() {
    struct sigaction term_action = {};
    struct sigaction int_action = {};
    if (sigaction(SIGTERM, nullptr, &term_action) != 0 ||
        sigaction(SIGINT, nullptr, &int_action) != 0) {
      return false;
    }
    return term_action.sa_handler == SIG_DFL && int_action.sa_handler == SIG_DFL;
  };
  // There is no injectable time source in SignalHandler; this bounded wall-clock poll is
  // inherent to observing an asynchronous disposition change made by its shutdown thread.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2); // NO_CHECK_FORMAT(real_time)
  while (!dispositions_restored() &&
         std::chrono::steady_clock::now() < deadline) {         // NO_CHECK_FORMAT(real_time)
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // NO_CHECK_FORMAT(real_time)
  }
  EXPECT_TRUE(dispositions_restored())
      << "second-signal escalation: SIGTERM/SIGINT dispositions were not restored to "
         "SIG_DFL after the first signal was handled, so a second Ctrl-C would still be "
         "silently swallowed";
}

// Regression death test: guards the dangling-delegate hazard from the process-lifetime
// angle. ~SignalHandler() restores the saved dispositions (typically SIG_DFL), so raising
// SIGTERM after destruction terminates the process. Before that fix, the stale static
// handler swallowed the signal (invoking the dangling delegate - UB that in practice
// wrote to a closed/cleared pipe and returned), the statement completed without dying,
// and this test failed with "failed to die".
//
// The "threadsafe" death test style is required: it re-executes the test binary for
// the child so the child starts single-threaded, which matters because SignalHandler
// spawns a shutdown thread (the default "fast" style just fork()s a process that may
// already have threads, which is unsafe/flaky here). This repo already uses death
// tests elsewhere (e.g. test/rate_limiter_test.cc).
TEST(SignalHandlerDeathTest, RaisingSignalAfterDestructionTerminatesProcess) {
  const std::string previous_style = GTEST_FLAG_GET(death_test_style);
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(
      {
        {
          SignalHandler signal_handler([]() {});
        }
        // With dispositions restored to SIG_DFL by the destructor above, this kills
        // the child process (default action for SIGTERM).
        std::raise(SIGTERM);
      },
      "");
  GTEST_FLAG_SET(death_test_style, previous_style);
}

} // namespace
} // namespace Nighthawk
