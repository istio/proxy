#include "quiche/common/quiche_callbacks.h"

#include <memory>
#include <utility>
#include <vector>

#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace {

void Apply(const std::vector<int>& container,
           UnretainedCallback<void(int)> function) {
  for (int n : container) {
    function(n);
  }
}

TEST(QuicheCallbacksTest, UnretainedCallback) {
  std::vector<int> nums = {1, 2, 3, 4};
  int sum = 0;
  Apply(nums, [&sum](int n) { sum += n; });
  EXPECT_EQ(sum, 10);
}

TEST(QuicheCallbacksTest, SingleUseCallback) {
  int called = 0;
  SingleUseCallback<void()> callback = [&called]() { called++; };
  EXPECT_EQ(called, 0);

  SingleUseCallback<void()> new_callback = std::move(callback);
  EXPECT_EQ(called, 0);

  std::move(new_callback)();
  EXPECT_EQ(called, 1);
  EXPECT_QUICHE_DEBUG_DEATH(
      std::move(new_callback)(),  // NOLINT(bugprone-use-after-move)
      "AnyInvocable");
}

class SetFlagOnDestruction {
 public:
  SetFlagOnDestruction(bool* flag) : flag_(flag) {}
  ~SetFlagOnDestruction() { *flag_ = true; }

 private:
  bool* flag_;
};

TEST(QuicheCallbacksTest, SingleUseCallbackOwnership) {
  bool deleted = false;
  auto flag_setter = std::make_unique<SetFlagOnDestruction>(&deleted);
  {
    SingleUseCallback<void()> callback = [setter = std::move(flag_setter)]() {};
    EXPECT_FALSE(deleted);
  }
  EXPECT_TRUE(deleted);
}

TEST(QuicheCallbacksTest, MultiUseCallback) {
  int called = 0;
  MultiUseCallback<void()> callback = [&called]() { called++; };
  EXPECT_EQ(called, 0);

  callback();
  EXPECT_EQ(called, 1);

  callback();
  callback();
  EXPECT_EQ(called, 3);
}

TEST(QuicheCallbacksTest, MultiUseCallbackOwnership) {
  bool deleted = false;
  auto flag_setter = std::make_unique<SetFlagOnDestruction>(&deleted);
  {
    MultiUseCallback<void()> callback = [setter = std::move(flag_setter)]() {};
    EXPECT_FALSE(deleted);
  }
  EXPECT_TRUE(deleted);
}

}  // namespace
}  // namespace quiche
