// This test covers the glob-style string pattern matching function,
// `glob_match`, defined in `glob.h`.

#include <datadog/glob.h>
#include <datadog/string_view.h>

#include "test.h"

using namespace datadog::tracing;

TEST_CASE("glob", "[glob]") {
  struct TestCase {
    StringView pattern;
    StringView subject;
    bool expected;
  };

  auto test_case = GENERATE(values<TestCase>(
      {// clang-format off
    // from the reference implementation
    // https://github.com/DataDog/tag-matching-sampling-rules/blob/master/glob.mjs
    {"foo", "foo", true},
    {"foo.*", "foo.you", true},
    {"foo.*", "snafoo.", false},
    {"hi*there", "hithere", true},
    {"*stuff", "lots of stuff", true},
    {"*stuff", "stuff to think about", false},
    {"*a*a*a*a*a*a", "aaaaaaaaaaaaaaaaaaaaaaaaaax", false},
    {"*a*a*a*a*a*a", "aaaaaaaarrrrrrraaaraaarararaarararaarararaaa", true},

    // from deliberation with Zach Groves
    {"aok*", "aok**", true},

    // question marks
    {"mysql??", "mysql01", true},
    {"mysql??", "mysql1x", true},
    {"n?-ingress-*", "ny-ingress-backup", true},
    {"n?-ingress-*", "nj-ingress-leader", true},
    {"n?-ingress-*", "nj-ingress", false},

    // edge cases
    {"", "", true},
    {"", "a", false},
    {"*", "", true},
    {"?", "", false},

    // case sensitivity
    {"true", "TRUE", true},
    {"true", "True", true},
    {"true", "tRue", true},
    {"false", "FALSE", true}
  }));
  // clang-format on

  CAPTURE(test_case.pattern);
  CAPTURE(test_case.subject);
  CAPTURE(test_case.expected);
  REQUIRE(glob_match(test_case.pattern, test_case.subject) ==
          test_case.expected);
}
