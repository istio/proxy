#include "quiche/common/platform/api/quiche_stack_trace.h"

#include <cstdint>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/strings/str_cat.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace test {
namespace {

bool ShouldRunTest() {
#if defined(ABSL_HAVE_ATTRIBUTE_NOINLINE)
  return QuicheShouldRunStackTraceTest();
#else
  // If QuicheDesignatedStackTraceTestFunction gets inlined, the test will
  // inevitably fail, since the function won't be on the stack trace.  Disable
  // the test in that scenario.
  return false;
#endif
}

ABSL_ATTRIBUTE_NOINLINE std::string QuicheDesignatedStackTraceTestFunction() {
  std::string result = QuicheStackTrace();
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
  return result;
}

ABSL_ATTRIBUTE_NOINLINE std::string
QuicheDesignatedTwoStepStackTraceTestFunction() {
  std::string result = SymbolizeStackTrace(CurrentStackTrace());
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
  return result;
}

TEST(QuicheStackTraceTest, GetStackTrace) {
  if (!ShouldRunTest()) {
    return;
  }

  std::string stacktrace = QuicheDesignatedStackTraceTestFunction();
  EXPECT_THAT(stacktrace,
              testing::HasSubstr("QuicheDesignatedStackTraceTestFunction"));
}

TEST(QuicheStackTraceTest, GetStackTraceInTwoSteps) {
  if (!ShouldRunTest()) {
    return;
  }

  std::string stacktrace = QuicheDesignatedTwoStepStackTraceTestFunction();
  EXPECT_THAT(stacktrace, testing::HasSubstr(
                              "QuicheDesignatedTwoStepStackTraceTestFunction"));
}

}  // namespace
}  // namespace test
}  // namespace quiche
