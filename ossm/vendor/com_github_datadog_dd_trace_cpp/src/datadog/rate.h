#pragma once

// This component provides a `class`, `Rate`, that is a `double` whose value is
// between zero and one, inclusive.
//
// `Rate` objects are obtained by calling the static member function
// `Rate::from` with a `double` argument. A default-constructed `Rate` has the
// zero value. The static member functions `Rate::one` and `Rate::zero` are
// provided for convenience.

#include <variant>

#include "expected.h"

namespace datadog {
namespace tracing {

class Rate {
  double value_;
  explicit Rate(double value) : value_(value) {}

 public:
  Rate() : value_(0.0) {}

  double value() const { return value_; }
  operator double() const { return value(); }

  static Rate one() { return Rate(1.0); }
  static Rate zero() { return Rate(0.0); }

  static Expected<Rate> from(double);
};

}  // namespace tracing
}  // namespace datadog
