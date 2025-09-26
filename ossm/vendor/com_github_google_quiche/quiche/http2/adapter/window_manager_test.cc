#include "quiche/http2/adapter/window_manager.h"

#include <algorithm>
#include <list>

#include "absl/functional/bind_front.h"
#include "quiche/http2/test_tools/http2_random.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {

// Use the peer to access private vars of WindowManager.
class WindowManagerPeer {
 public:
  explicit WindowManagerPeer(const WindowManager& wm) : wm_(wm) {}

  int64_t buffered() { return wm_.buffered_; }

 private:
  const WindowManager& wm_;
};

namespace {

class WindowManagerTest : public quiche::test::QuicheTest {
 protected:
  WindowManagerTest()
      : wm_(kDefaultLimit, absl::bind_front(&WindowManagerTest::OnCall, this)),
        peer_(wm_) {}

  void OnCall(int64_t s) { call_sequence_.push_back(s); }

  const int64_t kDefaultLimit = 32 * 1024 * 3;
  std::list<int64_t> call_sequence_;
  WindowManager wm_;
  WindowManagerPeer peer_;
  ::http2::test::Http2Random random_;
};

// A few no-op calls.
TEST_F(WindowManagerTest, NoOps) {
  wm_.SetWindowSizeLimit(kDefaultLimit);
  wm_.SetWindowSizeLimit(0);
  wm_.SetWindowSizeLimit(kDefaultLimit);
  wm_.MarkDataBuffered(0);
  wm_.MarkDataFlushed(0);
  EXPECT_TRUE(call_sequence_.empty());
}

// This test verifies that WindowManager does not notify its listener when data
// is only buffered, and never flushed.
TEST_F(WindowManagerTest, DataOnlyBuffered) {
  int64_t total = 0;
  while (total < kDefaultLimit) {
    int64_t s = std::min<int64_t>(kDefaultLimit - total, random_.Uniform(1024));
    total += s;
    wm_.MarkDataBuffered(s);
  }
  EXPECT_THAT(call_sequence_, ::testing::IsEmpty());
}

// This test verifies that WindowManager does notify its listener when data is
// buffered and subsequently flushed.
TEST_F(WindowManagerTest, DataBufferedAndFlushed) {
  int64_t total_buffered = 0;
  int64_t total_flushed = 0;
  while (call_sequence_.empty()) {
    int64_t buffered = std::min<int64_t>(kDefaultLimit - total_buffered,
                                         random_.Uniform(1024));
    wm_.MarkDataBuffered(buffered);
    total_buffered += buffered;
    EXPECT_TRUE(call_sequence_.empty());
    int64_t flushed = (total_buffered - total_flushed) > 0
                          ? random_.Uniform(total_buffered - total_flushed)
                          : 0;
    wm_.MarkDataFlushed(flushed);
    total_flushed += flushed;
  }
  // If WindowManager decided to send an update, at least one third of the
  // window must have been consumed by buffered data.
  EXPECT_GE(total_buffered, kDefaultLimit / 3);
}

// Window manager should avoid window underflow.
TEST_F(WindowManagerTest, AvoidWindowUnderflow) {
  EXPECT_EQ(wm_.CurrentWindowSize(), wm_.WindowSizeLimit());
  // Don't buffer more than the total window!
  wm_.MarkDataBuffered(wm_.WindowSizeLimit() + 1);
  EXPECT_EQ(wm_.CurrentWindowSize(), 0u);
}

// Window manager should GFE_BUG and avoid buffered underflow.
TEST_F(WindowManagerTest, AvoidBufferedUnderflow) {
  EXPECT_EQ(peer_.buffered(), 0u);
  // Don't flush more than has been buffered!
  EXPECT_QUICHE_BUG(wm_.MarkDataFlushed(1), "buffered underflow");
  EXPECT_EQ(peer_.buffered(), 0u);

  wm_.MarkDataBuffered(42);
  EXPECT_EQ(peer_.buffered(), 42u);
  // Don't flush more than has been buffered!
  EXPECT_QUICHE_BUG(
      {
        wm_.MarkDataFlushed(43);
        EXPECT_EQ(peer_.buffered(), 0u);
      },
      "buffered underflow");
}

// This test verifies that WindowManager notifies its listener when window is
// consumed (data is ignored or immediately dropped).
TEST_F(WindowManagerTest, WindowConsumed) {
  int64_t consumed = kDefaultLimit / 3 - 1;
  wm_.MarkWindowConsumed(consumed);
  EXPECT_TRUE(call_sequence_.empty());
  const int64_t extra = 1;
  wm_.MarkWindowConsumed(extra);
  EXPECT_THAT(call_sequence_, testing::ElementsAre(consumed + extra));
}

// This test verifies that WindowManager notifies its listener when the window
// size limit is increased.
TEST_F(WindowManagerTest, ListenerCalledOnSizeUpdate) {
  wm_.SetWindowSizeLimit(kDefaultLimit - 1024);
  EXPECT_TRUE(call_sequence_.empty());
  wm_.SetWindowSizeLimit(kDefaultLimit * 5);
  // Because max(outstanding window, previous limit) is kDefaultLimit, it is
  // only appropriate to increase the window by kDefaultLimit * 4.
  EXPECT_THAT(call_sequence_, testing::ElementsAre(kDefaultLimit * 4));
}

// This test verifies that when data is buffered and then the limit is
// decreased, WindowManager only notifies the listener once any outstanding
// window has been consumed.
TEST_F(WindowManagerTest, WindowUpdateAfterLimitDecreased) {
  wm_.MarkDataBuffered(kDefaultLimit - 1024);
  wm_.SetWindowSizeLimit(kDefaultLimit - 2048);

  // Now there are 2048 bytes of window outstanding beyond the current limit,
  // and we have 1024 bytes of data buffered beyond the current limit. This is
  // intentional, to be sure that WindowManager works properly if the limit is
  // decreased at runtime.

  wm_.MarkDataFlushed(512);
  EXPECT_TRUE(call_sequence_.empty());
  wm_.MarkDataFlushed(512);
  EXPECT_TRUE(call_sequence_.empty());
  wm_.MarkDataFlushed(512);
  EXPECT_TRUE(call_sequence_.empty());
  wm_.MarkDataFlushed(1024);
  EXPECT_THAT(call_sequence_, testing::ElementsAre(512));
}

// For normal behavior, we only call MaybeNotifyListener() when data is
// flushed. But if window runs out entirely, we still need to call
// MaybeNotifyListener() to avoid becoming artificially blocked when data isn't
// being flushed.
TEST_F(WindowManagerTest, ZeroWindowNotification) {
  // Consume a byte of window, but not enough to trigger an update.
  wm_.MarkWindowConsumed(1);

  // Buffer the remaining window.
  wm_.MarkDataBuffered(kDefaultLimit - 1);
  // Listener is notified of the remaining byte of possible window.
  EXPECT_THAT(call_sequence_, testing::ElementsAre(1));
}

TEST_F(WindowManagerTest, OnWindowSizeLimitChange) {
  wm_.MarkDataBuffered(10000);
  EXPECT_EQ(wm_.CurrentWindowSize(), kDefaultLimit - 10000);
  EXPECT_EQ(wm_.WindowSizeLimit(), kDefaultLimit);

  wm_.OnWindowSizeLimitChange(kDefaultLimit + 1000);
  EXPECT_EQ(wm_.CurrentWindowSize(), kDefaultLimit - 9000);
  EXPECT_EQ(wm_.WindowSizeLimit(), kDefaultLimit + 1000);

  wm_.OnWindowSizeLimitChange(kDefaultLimit - 1000);
  EXPECT_EQ(wm_.CurrentWindowSize(), kDefaultLimit - 11000);
  EXPECT_EQ(wm_.WindowSizeLimit(), kDefaultLimit - 1000);
}

TEST_F(WindowManagerTest, NegativeWindowSize) {
  wm_.MarkDataBuffered(80000);
  // 98304 window - 80000 buffered = 18304 available
  EXPECT_EQ(wm_.CurrentWindowSize(), 18304);
  wm_.OnWindowSizeLimitChange(65535);
  // limit decreases by 98304 - 65535 = 32769, window becomes -14465
  EXPECT_EQ(wm_.CurrentWindowSize(), -14465);
  wm_.MarkDataFlushed(70000);
  // Still 10000 bytes buffered, so window manager grants sufficient quota to
  // reach a window of 65535 - 10000.
  EXPECT_EQ(wm_.CurrentWindowSize(), 55535);
  // Desired window minus existing window: 55535 - (-14465) = 70000
  EXPECT_THAT(call_sequence_, testing::ElementsAre(70000));
}

TEST_F(WindowManagerTest, IncreaseWindow) {
  wm_.MarkDataBuffered(1000);
  EXPECT_EQ(wm_.CurrentWindowSize(), kDefaultLimit - 1000);
  EXPECT_EQ(wm_.WindowSizeLimit(), kDefaultLimit);

  // Increasing the window beyond the limit is allowed.
  wm_.IncreaseWindow(5000);
  EXPECT_EQ(wm_.CurrentWindowSize(), kDefaultLimit + 4000);
  EXPECT_EQ(wm_.WindowSizeLimit(), kDefaultLimit);

  // 80000 bytes are buffered, then flushed.
  wm_.MarkWindowConsumed(80000);
  // The window manager replenishes the consumed quota up to the limit.
  EXPECT_THAT(call_sequence_, testing::ElementsAre(75000));
  // The window is the limit, minus buffered data, as expected.
  EXPECT_EQ(wm_.CurrentWindowSize(), kDefaultLimit - 1000);
}

// This test verifies that when the constructor option is specified,
// WindowManager does not update its internal accounting of the flow control
// window when notifying the listener.
TEST(WindowManagerNoUpdateTest, NoWindowUpdateOnListener) {
  const int64_t kDefaultLimit = 65535;

  std::list<int64_t> call_sequence1;
  WindowManager wm1(
      kDefaultLimit,
      [&call_sequence1](int64_t delta) { call_sequence1.push_back(delta); },
      /*should_notify_listener=*/{},
      /*update_window_on_notify=*/true);  // default
  std::list<int64_t> call_sequence2;
  WindowManager wm2(
      kDefaultLimit,
      [&call_sequence2](int64_t delta) { call_sequence2.push_back(delta); },
      /*should_notify_listener=*/{},
      /*update_window_on_notify=*/false);

  const int64_t consumed = kDefaultLimit / 3 - 1;

  wm1.MarkWindowConsumed(consumed);
  EXPECT_TRUE(call_sequence1.empty());
  wm2.MarkWindowConsumed(consumed);
  EXPECT_TRUE(call_sequence2.empty());

  EXPECT_EQ(wm1.CurrentWindowSize(), kDefaultLimit - consumed);
  EXPECT_EQ(wm2.CurrentWindowSize(), kDefaultLimit - consumed);

  const int64_t extra = 1;
  wm1.MarkWindowConsumed(extra);
  EXPECT_THAT(call_sequence1, testing::ElementsAre(consumed + extra));
  // Window size *is* updated after invoking the listener.
  EXPECT_EQ(wm1.CurrentWindowSize(), kDefaultLimit);
  call_sequence1.clear();

  wm2.MarkWindowConsumed(extra);
  EXPECT_THAT(call_sequence2, testing::ElementsAre(consumed + extra));
  // Window size is *not* updated after invoking the listener.
  EXPECT_EQ(wm2.CurrentWindowSize(), kDefaultLimit - (consumed + extra));
  call_sequence2.clear();

  // Manually increase the window by the listener notification amount.
  wm2.IncreaseWindow(consumed + extra);
  EXPECT_EQ(wm2.CurrentWindowSize(), kDefaultLimit);

  wm1.SetWindowSizeLimit(kDefaultLimit * 5);
  EXPECT_THAT(call_sequence1, testing::ElementsAre(kDefaultLimit * 4));
  // *Does* update the window size.
  EXPECT_EQ(wm1.CurrentWindowSize(), kDefaultLimit * 5);

  wm2.SetWindowSizeLimit(kDefaultLimit * 5);
  EXPECT_THAT(call_sequence2, testing::ElementsAre(kDefaultLimit * 4));
  // Does *not* update the window size.
  EXPECT_EQ(wm2.CurrentWindowSize(), kDefaultLimit);
}

// This test verifies that when the constructor option is specified,
// WindowManager uses the provided ShouldWindowUpdateFn to determine when to
// notify the listener.
TEST(WindowManagerShouldUpdateTest, CustomShouldWindowUpdateFn) {
  const int64_t kDefaultLimit = 65535;

  // This window manager should always notify.
  std::list<int64_t> call_sequence1;
  WindowManager wm1(
      kDefaultLimit,
      [&call_sequence1](int64_t delta) { call_sequence1.push_back(delta); },
      [](int64_t /*limit*/, int64_t /*window*/, int64_t /*delta*/) {
        return true;
      });
  // This window manager should never notify.
  std::list<int64_t> call_sequence2;
  WindowManager wm2(
      kDefaultLimit,
      [&call_sequence2](int64_t delta) { call_sequence2.push_back(delta); },
      [](int64_t /*limit*/, int64_t /*window*/, int64_t /*delta*/) {
        return false;
      });
  // This window manager should notify as long as no data is buffered.
  std::list<int64_t> call_sequence3;
  WindowManager wm3(
      kDefaultLimit,
      [&call_sequence3](int64_t delta) { call_sequence3.push_back(delta); },
      [](int64_t limit, int64_t window, int64_t delta) {
        return delta == limit - window;
      });

  const int64_t consumed = kDefaultLimit / 4;

  wm1.MarkWindowConsumed(consumed);
  EXPECT_THAT(call_sequence1, testing::ElementsAre(consumed));
  wm2.MarkWindowConsumed(consumed);
  EXPECT_TRUE(call_sequence2.empty());
  wm3.MarkWindowConsumed(consumed);
  EXPECT_THAT(call_sequence3, testing::ElementsAre(consumed));

  const int64_t buffered = 42;

  wm1.MarkDataBuffered(buffered);
  EXPECT_THAT(call_sequence1, testing::ElementsAre(consumed));
  wm2.MarkDataBuffered(buffered);
  EXPECT_TRUE(call_sequence2.empty());
  wm3.MarkDataBuffered(buffered);
  EXPECT_THAT(call_sequence3, testing::ElementsAre(consumed));

  wm1.MarkDataFlushed(buffered / 3);
  EXPECT_THAT(call_sequence1, testing::ElementsAre(consumed, buffered / 3));
  wm2.MarkDataFlushed(buffered / 3);
  EXPECT_TRUE(call_sequence2.empty());
  wm3.MarkDataFlushed(buffered / 3);
  EXPECT_THAT(call_sequence3, testing::ElementsAre(consumed));

  wm1.MarkDataFlushed(2 * buffered / 3);
  EXPECT_THAT(call_sequence1,
              testing::ElementsAre(consumed, buffered / 3, 2 * buffered / 3));
  wm2.MarkDataFlushed(2 * buffered / 3);
  EXPECT_TRUE(call_sequence2.empty());
  wm3.MarkDataFlushed(2 * buffered / 3);
  EXPECT_THAT(call_sequence3, testing::ElementsAre(consumed, buffered));
}

}  // namespace
}  // namespace test
}  // namespace adapter
}  // namespace http2
