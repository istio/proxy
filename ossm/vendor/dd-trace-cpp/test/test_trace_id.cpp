// This test covers operations defined for `class TraceID` in `trace_id.h`.

#include <datadog/error.h>
#include <datadog/optional.h>
#include <datadog/trace_id.h>

#include "test.h"

using namespace datadog::tracing;

TEST_CASE("TraceID defaults to zero") {
  TraceID id1;
  REQUIRE(id1.low == 0);
  REQUIRE(id1.high == 0);

  TraceID id2{0xdeadbeef};
  REQUIRE(id2.low == 0xdeadbeef);
  REQUIRE(id2.high == 0);
}

TEST_CASE("TraceID parsed from hexadecimal") {
  struct TestCase {
    int line;
    std::string input;
    Optional<TraceID> expected_id;
    Optional<Error::Code> expected_error = nullopt;
  };

  // clang-format off
  const auto test_case = GENERATE(values<TestCase>({
        {__LINE__, "00001", TraceID(1)},
        {__LINE__, "0000000000000000000000000000000000000000000001", TraceID(1)},
        {__LINE__, "", nullopt, Error::INVALID_INTEGER},
        {__LINE__, "nonsense", nullopt, Error::INVALID_INTEGER},
        {__LINE__, "1000000000000000000000000000000000000000000000", nullopt, Error::OUT_OF_RANGE_INTEGER},
        {__LINE__, "deadbeefdeadbeef", TraceID{0xdeadbeefdeadbeefULL}},
        {__LINE__, "0xdeadbeefdeadbeef", nullopt, Error::INVALID_INTEGER},
        {__LINE__, "cafebabecafebabedeadbeefdeadbeef", TraceID{0xdeadbeefdeadbeefULL, 0xcafebabecafebabe}},
        {__LINE__, "caxxxxxxcafebabedeadbeefdeadbeef", nullopt, Error::INVALID_INTEGER},
        {__LINE__, "cafebabecafebabedeaxxxxxxxxdbeef", nullopt, Error::INVALID_INTEGER},
  }));
  // clang-format on

  CAPTURE(test_case.line);
  CAPTURE(test_case.input);
  const auto result = TraceID::parse_hex(test_case.input);
  if (test_case.expected_error) {
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == *test_case.expected_error);
  } else {
    REQUIRE(result);
    REQUIRE(*result == *test_case.expected_id);
  }
}

TEST_CASE("TraceID comparisons") {
  // First, comparing integers with the `TraceID.low`.
  REQUIRE(TraceID{12345} == 12345);
  REQUIRE_FALSE(TraceID{12345} != 12345);

  REQUIRE(TraceID{12345} != 54321);
  REQUIRE_FALSE(TraceID{12345} == 54321);

  REQUIRE(TraceID{6789, 12345} != 12345);
  REQUIRE_FALSE(TraceID{6789, 12345} == 12345);

  // And the opposite argument order.
  REQUIRE(12345 == TraceID{12345});
  REQUIRE_FALSE(12345 != TraceID{12345});

  REQUIRE(54321 != TraceID{12345});
  REQUIRE_FALSE(54321 == TraceID{12345});

  REQUIRE(12345 != TraceID{6789, 12345});
  REQUIRE_FALSE(12345 == TraceID{6789, 12345});

  // Second, comparing trace IDs with other trace IDs.
  struct TestCase {
    int line;
    std::string name;
    TraceID left;
    TraceID right;
    bool equal;
  };

  // clang-format off
  const auto test_case = GENERATE(values<TestCase>({
    {__LINE__, "defaults", TraceID{}, TraceID{}, true},
    {__LINE__, "lowers equal", TraceID{0xcafebabe}, TraceID{0xcafebabe}, true},
    {__LINE__, "lowers not equal", TraceID{0xcafebabe}, TraceID{0xdeadbeef}, false},
    {__LINE__, "highers zeroness agree", TraceID{0xcafebabe, 0xdeadbeef}, TraceID{0xcafebabe, 0xdeadbeef}, true},
    {__LINE__, "highers zeroness disagree", TraceID{0xdeadbeef}, TraceID{0xcafebabe, 0xdeadbeef}, false},
    {__LINE__, "highers disagree", TraceID{0xdeadbeef, 0xdeadbeef}, TraceID{0xcafebabe, 0xdeadbeef}, false},
  }));
  // clang-format on

  CAPTURE(test_case.line);
  CAPTURE(test_case.name);
  if (test_case.equal) {
    REQUIRE(test_case.left == test_case.right);
    REQUIRE_FALSE(test_case.left != test_case.right);
  } else {
    REQUIRE_FALSE(test_case.left == test_case.right);
    REQUIRE(test_case.left != test_case.right);
  }
}

TEST_CASE("TraceID serialization") {
  struct TestCase {
    int line;
    std::string trace_id_source;
    TraceID trace_id;
    std::string expected_hex;
  };

#define CASE(TRACE_ID, HEX) \
  { __LINE__, #TRACE_ID, TRACE_ID, HEX }
  // clang-format off
  const auto test_case = GENERATE(values<TestCase>({
    CASE(TraceID(), "00000000000000000000000000000000"),
    CASE(TraceID(16), "00000000000000000000000000000010"),
    CASE(TraceID(0xcafebabe), "000000000000000000000000cafebabe"),
    CASE(TraceID(0, 1), "00000000000000010000000000000000"),
    CASE(TraceID(15, 0xcafebabe), "00000000cafebabe000000000000000f"),
  }));
// clang-format on
#undef CASE

  CAPTURE(test_case.line);
  CAPTURE(test_case.trace_id_source);
  REQUIRE(test_case.trace_id.hex_padded() == test_case.expected_hex);
}
