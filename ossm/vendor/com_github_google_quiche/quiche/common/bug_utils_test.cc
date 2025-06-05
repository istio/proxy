#include "quiche/common/bug_utils.h"

#include <string.h>

#include <algorithm>
#include <memory>
#include <string>

#include "absl/base/log_severity.h"
#include "absl/strings/string_view.h"
#include "quiche/common/bug_utils_test_helper.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace internal {
namespace {

using ::testing::_;
using ::testing::EndsWith;
using ::testing::InSequence;

class BugHandler {
 public:
  virtual ~BugHandler() = default;
  virtual void OnBug(absl::string_view file, int line,
                     absl::string_view message) = 0;
};

// This class provides a convenient way to write expectations for the bug
// override function.
class MockBugHandler : public BugHandler {
 public:
  MockBugHandler() = default;

  MOCK_METHOD(void, OnBug,
              (absl::string_view file, int line, absl::string_view message),
              (override));
};

MockBugHandler* mock_bug_handler = nullptr;

MockBugHandler* GetInstance() {
  if (mock_bug_handler == nullptr) {
    mock_bug_handler = new MockBugHandler;
  }
  return mock_bug_handler;
}

void ResetInstance() {
  delete mock_bug_handler;
  mock_bug_handler = nullptr;
}

class BugUtilsTest : public ::quiche::test::QuicheTest {
 public:
  void SetUp() override {
    fn_ = GenericBugStreamHandler::GetOverrideFunction();
    GenericBugStreamHandler::SetOverrideFunction(
        [](absl::LogSeverity /*severity*/, const char* file, int line,
           absl::string_view log_message) {
          GetInstance()->OnBug(absl::string_view(file, strlen(file)), line,
                               log_message);
        });
  }

  void TearDown() override {
    GenericBugStreamHandler::SetOverrideFunction(fn_);
    ResetInstance();
  }

  inline static GenericBugStreamHandler::OverrideFunction fn_ = nullptr;
};

// Tests several permutations.
TEST_F(BugUtilsTest, TestsEverythingUsing23And26) {
  InSequence seq;
  EXPECT_CALL(*GetInstance(), OnBug(EndsWith("bug_utils_test_helper.h"), 23,
                                    EndsWith("Here on line 23")));
  LogBugLine23();

  EXPECT_CALL(*GetInstance(), OnBug(EndsWith("bug_utils_test_helper.h"), 23,
                                    EndsWith("Here on line 23")));
  LogBugLine23();

  EXPECT_CALL(*GetInstance(), OnBug(EndsWith("bug_utils_test_helper.h"), 26,
                                    EndsWith("Here on line 26")));
  EXPECT_CALL(*GetInstance(), OnBug(EndsWith("bug_utils_test_helper.h"), 27,
                                    EndsWith("And 27!")));
  LogBugLine26();
}

TEST_F(BugUtilsTest, TestBugIf) {
  InSequence seq;

  // Verify that we don't invoke the function for a false condition.
  LogIfBugLine31(false);

  // The first true should trigger an invocation.
  EXPECT_CALL(*GetInstance(), OnBug(EndsWith("bug_utils_test_helper.h"), 31,
                                    EndsWith("Here on line 31")));
  LogIfBugLine31(true);

  // It's always a no-op if the condition is false.
  LogIfBugLine31(false);  // no-op
  LogIfBugLine31(false);  // no-op
}

TEST_F(BugUtilsTest, TestBugIfMessage) {
  int i;

  // Check success
  LogIfBugNullCheckLine35(&i);

  // Check failure
  EXPECT_CALL(
      *GetInstance(),
      OnBug(
          EndsWith("bug_utils_test_helper.h"), 35,
          EndsWith(
              "QUICHE_TEST_BUG_IF(Bug 35, ptr == nullptr): Here on line 35")));
  LogIfBugNullCheckLine35(nullptr);
}

// Don't actually need to crash, just cause a side effect the test can assert
// on.
int num_times_called = 0;
bool BadCondition() {
  ++num_times_called;
  return true;
}

TEST_F(BugUtilsTest, BadCondition) {
  InSequence seq;

  EXPECT_EQ(num_times_called, 0);

  EXPECT_CALL(*GetInstance(), OnBug(_, _, EndsWith("Called BadCondition")));
  QUICHE_TEST_BUG_IF(id, BadCondition()) << "Called BadCondition";
  EXPECT_EQ(num_times_called, 1);
}

TEST_F(BugUtilsTest, NoDanglingElse) {
  auto unexpected_bug_message = [] {
    ADD_FAILURE() << "This should not be called";
    return "bad";
  };

  if (false) QUICHE_TEST_BUG(dangling_else) << unexpected_bug_message();

  bool expected_else_reached = false;
  if (false)
    QUICHE_TEST_BUG(dangling_else_2) << unexpected_bug_message();
  else
    expected_else_reached = true;

  EXPECT_TRUE(expected_else_reached);
}

TEST_F(BugUtilsTest, BugListener) {
  class TestListener : public GenericBugListener {
   public:
    explicit TestListener(bool expect_log_message)
        : expect_log_message_(expect_log_message) {}

    ~TestListener() override { EXPECT_EQ(hit_count_, 1); }

    void OnBug(const char* bug_id, const char* file, int line,
               absl::string_view bug_message) override {
      ++hit_count_;
      EXPECT_EQ(bug_id, "bug_listener_test");
      EXPECT_EQ(file, __FILE__);
      EXPECT_GT(line, 0);
      if (expect_log_message_) {
        EXPECT_EQ(bug_message, "TEST_BUG(bug_listener_test): Bug listener msg");
      } else {
        EXPECT_EQ(bug_message, "");
      }
    }

    TestListener* self() { return this; }

   private:
    int hit_count_ = 0;
    const bool expect_log_message_;
  };

  GENERIC_BUG_IMPL("TEST_BUG", bug_listener_test, /*skip_log_condition=*/false,
                   QUICHE_TEST_BUG_OPTIONS().SetBugListener(
                       TestListener(/*expect_log_message=*/true).self()))
      << "Bug listener msg";

  GENERIC_BUG_IMPL("TEST_BUG", bug_listener_test, /*skip_log_condition=*/true,
                   QUICHE_TEST_BUG_OPTIONS().SetBugListener(
                       TestListener(/*expect_log_message=*/false).self()))
      << "Bug listener msg";
}

}  // namespace
}  // namespace internal
}  // namespace quiche
