#include "dragonbox/dragonbox.h"

constexpr auto x = jkj::dragonbox::to_decimal(3.1415);
static_assert(x.significand == 31415);
static_assert(x.exponent == -4);
static_assert(!x.is_negative);

constexpr auto y = jkj::dragonbox::to_decimal(123.);
static_assert(y.significand == 123);
static_assert(y.exponent == 0);
static_assert(!y.is_negative);

int main() {
    return 0;
}
