#include "glob.h"

#include <cctype>
#include <cstdint>

namespace datadog {
namespace tracing {

bool glob_match(StringView pattern, StringView subject) {
  // This is a backtracking implementation of the glob matching algorithm.
  // The glob pattern language supports `*` and `?`, but no escape sequences.
  //
  // Based off of a Go example in <https://research.swtch.com/glob> accessed
  // February 3, 2022.

  using Index = std::size_t;
  Index p = 0;       // [p]attern index
  Index s = 0;       // [s]ubject index
  Index next_p = 0;  // next [p]attern index
  Index next_s = 0;  // next [s]ubject index

  const size_t p_size = pattern.size();
  const size_t s_size = subject.size();

  while (p < p_size || s < s_size) {
    if (p < p_size) {
      const char pattern_char = pattern[p];
      switch (pattern_char) {
        case '*':
          // Try to match at `s`.  If that doesn't work out, restart at
          // `s + 1` next.
          next_p = p;
          next_s = s + 1;
          ++p;
          continue;
        case '?':
          if (s < s_size) {
            ++p;
            ++s;
            continue;
          }
          break;
        default:
          if (s < s_size && tolower(subject[s]) == tolower(pattern_char)) {
            ++p;
            ++s;
            continue;
          }
      }
    }
    // Mismatch.  Maybe restart.
    if (0 < next_s && next_s <= s_size) {
      p = next_p;
      s = next_s;
      continue;
    }
    return false;
  }
  return true;
}

}  // namespace tracing
}  // namespace datadog
