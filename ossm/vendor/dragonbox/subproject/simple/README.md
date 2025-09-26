# Simple Dragonbox

This is a simplified implementation of the algorithm, that closely follows the
one in `include/dragonbox/dragonbox.h`, but aims to be shorter overall, use less
C++ template indirection, and offer less flexibility and performance for the
sake of simplicity.

Simplifications over the implementation in `include/dragonbox/dragonbox.h` are
made based primarily on the following assumptions:

- No need to support `sign` policies (always uses `return_sign`).
- No need to support `trailing_zeros` policies (always uses `remove`).
- No need to support 128-bit compiler intrinsics (always uses portable
  implementation).
- No need to support the `fast` digit-generation policy (always uses `compact`).
- `if constexpr` is available (C++17).
- `float` and `double` use [IEEE-754 binary32](https://en.wikipedia.org/wiki/Single-precision_floating-point_format) and [IEEE-754 binary64](https://en.wikipedia.org/wiki/Double-precision_floating-point_format) representations,
  respectively.
- A modern compiler and standard library are available.

Note the `cache`, `binary_to_decimal_rounding`, and `decimal_to_binary_rounding`
policies are still supported.

# Usage Examples

Simple string generation from `float`/`double` (mirrors the interface and
behavior of `jkj::dragonbox::to_chars`; see the [primary README](/README.md)):

```cpp
#include "simple_dragonbox.h"

constexpr int buffer_length = 1 + // for '\0'
  simple_dragonbox::max_output_string_length<double>;
double x = 1.234;  // Also works for float
char buffer[buffer_length];

// Null-terminate the buffer and return the pointer to the null character.
// Hence, the length of the string is `end - buffer`.
// `buffer` is now { '1', '.', '2', '3', '4', 'E', '0', '\0', (garbage) }.
char* end = simple_dragonbox::to_chars(x, buffer);
```

Direct use of `simple_dragonbox::to_decimal` (mirrors the interface and
behavior of `jkj::dragonbox::to_decimal`; see the [primary README](/README.md)):

```cpp
#include "simple_dragonbox.h"
double x = 1.234;   // Also works for float

// `x` must be a nonzero finite number.
// `v` is a struct with three members:
// significand : decimal significand (1234 in this case);
//               it is of type std::uint64_t for double, std::uint32_t for float
//    exponent : decimal exponent (-3 in this case); it is of type int
//        sign : as the name suggests; it is of type bool
auto v = jkj::dragonbox::to_decimal(x);
```

Like `jkj::dragonbox::to_decimal`, `simple_dragonbox::to_decimal` only works
with finite nonzero inputs. Its behavior when given infinities, NaNs, and ±0 is
undefined. `simple_dragonbox::to_chars` works fine for any inputs.

# Policies

`simple_dragonbox` supports the same style of policy interface as
`jkj::dragonbox`. For an introduction to `jkj::dragonbox`'s policy interface,
see the [primary README](/README.md). For example,

```cpp
simple_dragonbox::to_decimal(3.14,
    simple_dragonbox::policy::cache::compact,
    simple_dragonbox::policy::binary_to_decimal_rounding::to_odd);
```

`simple_dragonbox` supports a subset of the policies that `jkj::dragonbox` does.
When supported, policies have the same meaning as the matching policy in
`jkj::dragonbox` (see the [primary README](/README.md)). Supported policies
include:

- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_to_even`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_to_odd`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_toward_plus_infinity`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_toward_minus_infinity`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_toward_zero`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_away_from_zero`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_to_even_static_boundary`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_to_odd_static_boundary`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_toward_plus_infinity_static_boundary`
- `simple_dragonbox::policy::decimal_to_binary_rounding::nearest_toward_minus_infinity_static_boundary`
- `simple_dragonbox::policy::decimal_to_binary_rounding::toward_plus_infinity`
- `simple_dragonbox::policy::decimal_to_binary_rounding::toward_minus_infinity`
- `simple_dragonbox::policy::decimal_to_binary_rounding::toward_zero`
- `simple_dragonbox::policy::decimal_to_binary_rounding::away_from_zero`

- `simple_dragonbox::policy::binary_to_decimal_rounding::to_even`
- `simple_dragonbox::policy::binary_to_decimal_rounding::to_odd`
- `simple_dragonbox::policy::binary_to_decimal_rounding::away_from_zero`
- `simple_dragonbox::policy::binary_to_decimal_rounding::toward_zero`
- `simple_dragonbox::policy::binary_to_decimal_rounding::do_not_care`

- `simple_dragonbox::policy::cache::full`
- `simple_dragonbox::policy::cache::compact`

# Internal direct API

Internally, `simple_dragonbox` uses a more explicit, direct interface to express
the various policies, via a class called `simple_dragonbox::detail::impl`. The
`impl` class has four template parameters: `Float`, `BinaryRoundMode`,
`DecimalRoundMode`, and `CacheType`, which correspond to the floating point
type and three categories of policies above. These template parameters can be
specified explicitly to instantiate a particular variant of the algorithm. The
class has a single constructor that receives a value of type `Float` (e.g.
`float` or `double`), and has methods `to_decimal()` and `to_chars()` for
performing the desired conversions.

For example,

```cpp
namespace detail = simple_dragonbox::detail;
using impl = detail::impl<double,
                          detail::binary_round_mode::toward_zero,
                          detail::decimal_round_mode::toward_zero,
                          detail::cache_type::compact>;

char buffer[32];
auto x = impl(3.14);
x.to_decimal();  // equivalent to `simple_dragonbox::to_decimal(3.14, policies...)`
x.to_chars(buffer);  // equivalent to `simple_dragonbox::to_chars(3.14, buf, policies...)`
```
