#include <datadog/error.h>
#include <datadog/parse_util.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>

#include "test.h"

using namespace datadog::tracing;

#define PARSE_UTIL_TEST(x) TEST_CASE(x, "[parse_util]")

PARSE_UTIL_TEST("parse_int") {
  struct TestCase {
    int line;
    std::string name;
    std::string argument;
    int base;
    std::variant<int, Error::Code> expected;
  };

  // clang-format off
  auto test_case = GENERATE(values<TestCase>({
      {__LINE__, "zero (dec)", "0", 10, 0},
      {__LINE__, "zeros (dec)", "000", 10, 0},
      {__LINE__, "zero (hex)", "0", 16, 0},
      {__LINE__, "zeros (hex)", "000", 16, 0},
      {__LINE__, "leading whitespace (dec 1)", " 42", 10, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (dec 2)", "\t42", 10, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (dec 3)", "\n42", 10, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (dec 1)", "42 ", 10, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (dec 2)", "42\t", 10, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (dec 3)", "42\n", 10, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (hex 1)", " 42", 16, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (hex 2)", "\t42", 16, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (hex 3)", "\n42", 16, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (hex 1)", "42 ", 16, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (hex 2)", "42\t", 16, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (hex 3)", "42\n", 16, Error::INVALID_INTEGER},
      {__LINE__, "no hex prefix", "0xbeef", 16, Error::INVALID_INTEGER},
      {__LINE__, "dec rejects hex", "42beef", 10, Error::INVALID_INTEGER},
      {__LINE__, "hex accepts hex", "42beef", 16, 0x42beef},
      {__LINE__, "no trailing nonsense (dec)", "42xyz", 10, Error::INVALID_INTEGER},
      {__LINE__, "no trailing nonsense (hex)", "42xyz", 16, Error::INVALID_INTEGER},
      {__LINE__, "no leading nonsense (dec)", "xyz42", 10, Error::INVALID_INTEGER},
      {__LINE__, "no leading nonsense (hex)", "xyz42", 16, Error::INVALID_INTEGER},
      {__LINE__, "overflow", std::to_string(std::numeric_limits<int>::max()) + "0", 10, Error::OUT_OF_RANGE_INTEGER},
      {__LINE__, "negative (dec)", "-10", 10, -10},
      {__LINE__, "negative (hex)", "-a", 16, -10},
      {__LINE__, "lower case", "a", 16, 10},
      {__LINE__, "upper case", "A", 16, 10},
      {__LINE__, "underflow", std::to_string(std::numeric_limits<int>::min()) + "0", 10, Error::OUT_OF_RANGE_INTEGER},
  }));
  // clang-format on

  CAPTURE(test_case.line);
  CAPTURE(test_case.name);
  CAPTURE(test_case.argument);
  CAPTURE(test_case.base);

  const auto result = parse_int(test_case.argument, test_case.base);
  if (std::holds_alternative<int>(test_case.expected)) {
    const int& expected = std::get<int>(test_case.expected);
    REQUIRE(result);
    REQUIRE(*result == expected);
  } else {
    assert(std::holds_alternative<Error::Code>(test_case.expected));
    const Error::Code& expected = std::get<Error::Code>(test_case.expected);
    REQUIRE(!result);
    REQUIRE(result.error().code == expected);
  }
}

// This test case is similar to the one above, except that negative numbers are
// not supported, and the underflow and overflow values are different.
PARSE_UTIL_TEST("parse_uint64") {
  struct TestCase {
    int line;
    std::string name;
    std::string argument;
    int base;
    std::variant<std::uint64_t, Error::Code> expected;
  };

  // clang-format off
  auto test_case = GENERATE(values<TestCase>({
      {__LINE__, "zero (dec)", "0", 10, UINT64_C(0)},
      {__LINE__, "zeros (dec)", "000", 10, UINT64_C(0)},
      {__LINE__, "zero (hex)", "0", 16, UINT64_C(0)},
      {__LINE__, "zeros (hex)", "000", 16, UINT64_C(0)},
      {__LINE__, "leading whitespace (dec 1)", " 42", 10, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (dec 2)", "\t42", 10, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (dec 3)", "\n42", 10, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (dec 1)", "42 ", 10, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (dec 2)", "42\t", 10, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (dec 3)", "42\n", 10, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (hex 1)", " 42", 16, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (hex 2)", "\t42", 16, Error::INVALID_INTEGER},
      {__LINE__, "leading whitespace (hex 3)", "\n42", 16, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (hex 1)", "42 ", 16, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (hex 2)", "42\t", 16, Error::INVALID_INTEGER},
      {__LINE__, "trailing whitespace (hex 3)", "42\n", 16, Error::INVALID_INTEGER},
      {__LINE__, "no hex prefix", "0xbeef", 16, Error::INVALID_INTEGER},
      {__LINE__, "dec rejects hex", "42beef", 10, Error::INVALID_INTEGER},
      {__LINE__, "hex accepts hex", "42beef", 16, UINT64_C(0x42beef)},
      {__LINE__, "no trailing nonsense (dec)", "42xyz", 10, Error::INVALID_INTEGER},
      {__LINE__, "no trailing nonsense (hex)", "42xyz", 16, Error::INVALID_INTEGER},
      {__LINE__, "no leading nonsense (dec)", "xyz42", 10, Error::INVALID_INTEGER},
      {__LINE__, "no leading nonsense (hex)", "xyz42", 16, Error::INVALID_INTEGER},
      {__LINE__, "overflow", std::to_string(std::numeric_limits<std::uint64_t>::max()) + "0", 10, Error::OUT_OF_RANGE_INTEGER},
      {__LINE__, "negative (dec)", "-10", 10, Error::INVALID_INTEGER},
      {__LINE__, "negative (hex)", "-a", 16, Error::INVALID_INTEGER},
      {__LINE__, "lower case", "a", 16, UINT64_C(10)},
      {__LINE__, "upper case", "A", 16, UINT64_C(10)},
  }));
  // clang-format on

  CAPTURE(test_case.line);
  CAPTURE(test_case.name);
  CAPTURE(test_case.argument);
  CAPTURE(test_case.base);

  const auto result = parse_uint64(test_case.argument, test_case.base);
  if (std::holds_alternative<std::uint64_t>(test_case.expected)) {
    const std::uint64_t& expected = std::get<std::uint64_t>(test_case.expected);
    REQUIRE(result);
    REQUIRE(*result == expected);
  } else {
    assert(std::holds_alternative<Error::Code>(test_case.expected));
    const Error::Code& expected = std::get<Error::Code>(test_case.expected);
    REQUIRE(!result);
    REQUIRE(result.error().code == expected);
  }
}

PARSE_UTIL_TEST("parse_tags") {
  struct TestCase {
    int line;
    StringView name;
    StringView input;
    std::unordered_map<std::string, std::string> expected;
  };

  auto test_case = GENERATE(values<TestCase>({
      {__LINE__,
       "space separated tags",
       "env:test aKey:aVal bKey:bVal cKey:",
       {
           {"env", "test"},
           {"aKey", "aVal"},
           {"bKey", "bVal"},
           {"cKey", ""},
       }},
      {__LINE__,
       "comma separated tags",
       "env:test aKey:aVal bKey:bVal cKey:",
       {
           {"env", "test"},
           {"aKey", "aVal"},
           {"bKey", "bVal"},
           {"cKey", ""},
       }},
      {__LINE__,
       "mixed separator but comma first",
       "env:test,aKey:aVal bKey:bVal cKey:",
       {
           {"env", "test"},
           {"aKey", "aVal bKey:bVal cKey:"},
       }},
      {__LINE__,
       "mixed separator but space first",
       "env:test     bKey :bVal dKey: dVal cKey:",
       {
           {"env", "test"},
           {"bKey", ""},
           {"dKey", ""},
           {"dVal", ""},
           {"cKey", ""},
       }},
      {__LINE__,
       "mixed separator but space first",
       "env:keyWithA:Semicolon bKey:bVal cKey",
       {
           {"env", "keyWithA:Semicolon"},
           {"bKey", "bVal"},
           {"cKey", ""},
       }},
      // {__LINE__,
      //  "mixed separator edge case",
      //  "env:keyWith:  , ,   Lots:Of:Semicolons ",
      //  {
      //      {"env", "keyWith:"},
      //      {"Lots", "Of:Semicolons"},
      //  }},
      {__LINE__,
       "comma separated but some tags without value",
       "a:b,c,d",
       {
           {"a", "b"},
           {"c", ""},
           {"d", ""},
       }},
      {__LINE__,
       "one separator without value",
       "a,1",
       {
           {"a", ""},
           {"1", ""},
       }},
      {__LINE__,
       "no separator",
       "a:b:c:d",
       {
           {"a", "b:c:d"},
       }},
      {__LINE__,
       "input is trimed",
       "key1:val1, key2 : val2 ",
       {
           {"key1", "val1"},
           {"key2", "val2"},
       }},
  }));

  CAPTURE(test_case.line);
  CAPTURE(test_case.name);
  CAPTURE(test_case.input);

  const auto tags = parse_tags(test_case.input);
  REQUIRE(tags);
  CHECK(*tags == test_case.expected);
}
