#include "quiche/http2/test_tools/hpack_example.h"

#include <string>

// Tests of HpackExampleToStringOrDie.

#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace test {
namespace {

TEST(HpackExampleToStringOrDie, GoodInput) {
  std::string bytes = HpackExampleToStringOrDie(R"(
      40                                      | == Literal never indexed ==
                                              | Blank lines are OK in example:

      08                                      |   Literal name (len = 8)
      7061 7373 776f 7264                     | password
      06                                      |   Literal value (len = 6)
      7365 6372 6574                          | secret
                                              | -> password: secret
      )");

  // clang-format off
  const char kExpected[] = {
    0x40,                      // Never Indexed, Literal Name and Value
    0x08,                      //  Name Len: 8
    0x70, 0x61, 0x73, 0x73,    //      Name: password
    0x77, 0x6f, 0x72, 0x64,    //
    0x06,                      // Value Len: 6
    0x73, 0x65, 0x63, 0x72,    //     Value: secret
    0x65, 0x74,                //
  };
  // clang-format on
  EXPECT_EQ(absl::string_view(kExpected, sizeof kExpected), bytes);
}

#ifdef GTEST_HAS_DEATH_TEST
TEST(HpackExampleToStringOrDie, InvalidInput) {
  EXPECT_QUICHE_DEATH(HpackExampleToStringOrDie("4"), "Truncated");
  EXPECT_QUICHE_DEATH(HpackExampleToStringOrDie("4x"), "half");
  EXPECT_QUICHE_DEATH(HpackExampleToStringOrDie(""), "empty");
}
#endif

}  // namespace
}  // namespace test
}  // namespace http2
