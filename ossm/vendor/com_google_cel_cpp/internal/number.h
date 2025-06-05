// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_NUMBER_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_NUMBER_H_

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/types/variant.h"

namespace cel::internal {

constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
constexpr int64_t kInt64Min = std::numeric_limits<int64_t>::lowest();
constexpr uint64_t kUint64Max = std::numeric_limits<uint64_t>::max();
constexpr uint64_t kUintToIntMax = static_cast<uint64_t>(kInt64Max);
constexpr double kDoubleToIntMax = static_cast<double>(kInt64Max);
constexpr double kDoubleToIntMin = static_cast<double>(kInt64Min);
constexpr double kDoubleToUintMax = static_cast<double>(kUint64Max);

// The highest integer values that are round-trippable after rounding and
// casting to double.
template <typename T>
constexpr int RoundingError() {
  return 1 << (std::numeric_limits<T>::digits -
               std::numeric_limits<double>::digits - 1);
}

constexpr double kMaxDoubleRepresentableAsInt =
    static_cast<double>(kInt64Max - RoundingError<int64_t>());
constexpr double kMaxDoubleRepresentableAsUint =
    static_cast<double>(kUint64Max - RoundingError<uint64_t>());

#define CEL_ABSL_VISIT_CONSTEXPR

using NumberVariant = absl::variant<double, uint64_t, int64_t>;

enum class ComparisonResult {
  kLesser,
  kEqual,
  kGreater,
  // Special case for nan.
  kNanInequal
};

// Return the inverse relation (i.e. Invert(cmp(b, a)) is the same as cmp(a, b).
constexpr ComparisonResult Invert(ComparisonResult result) {
  switch (result) {
    case ComparisonResult::kLesser:
      return ComparisonResult::kGreater;
    case ComparisonResult::kGreater:
      return ComparisonResult::kLesser;
    case ComparisonResult::kEqual:
      return ComparisonResult::kEqual;
    case ComparisonResult::kNanInequal:
      return ComparisonResult::kNanInequal;
  }
}

template <typename OutType>
struct ConversionVisitor {
  template <typename InType>
  constexpr OutType operator()(InType v) {
    return static_cast<OutType>(v);
  }
};

template <typename T>
constexpr ComparisonResult Compare(T a, T b) {
  return (a > b)    ? ComparisonResult::kGreater
         : (a == b) ? ComparisonResult::kEqual
                    : ComparisonResult::kLesser;
}

constexpr ComparisonResult DoubleCompare(double a, double b) {
  // constexpr friendly isnan check.
  if (!(a == a) || !(b == b)) {
    return ComparisonResult::kNanInequal;
  }
  return Compare(a, b);
}

// Implement generic numeric comparison against double value.
struct DoubleCompareVisitor {
  constexpr explicit DoubleCompareVisitor(double v) : v(v) {}

  constexpr ComparisonResult operator()(double other) const {
    return DoubleCompare(v, other);
  }

  constexpr ComparisonResult operator()(uint64_t other) const {
    if (v > kDoubleToUintMax) {
      return ComparisonResult::kGreater;
    } else if (v < 0) {
      return ComparisonResult::kLesser;
    } else {
      return DoubleCompare(v, static_cast<double>(other));
    }
  }

  constexpr ComparisonResult operator()(int64_t other) const {
    if (v > kDoubleToIntMax) {
      return ComparisonResult::kGreater;
    } else if (v < kDoubleToIntMin) {
      return ComparisonResult::kLesser;
    } else {
      return DoubleCompare(v, static_cast<double>(other));
    }
  }
  double v;
};

// Implement generic numeric comparison against uint value.
// Delegates to double comparison if either variable is double.
struct UintCompareVisitor {
  constexpr explicit UintCompareVisitor(uint64_t v) : v(v) {}

  constexpr ComparisonResult operator()(double other) const {
    return Invert(DoubleCompareVisitor(other)(v));
  }

  constexpr ComparisonResult operator()(uint64_t other) const {
    return Compare(v, other);
  }

  constexpr ComparisonResult operator()(int64_t other) const {
    if (v > kUintToIntMax || other < 0) {
      return ComparisonResult::kGreater;
    } else {
      return Compare(v, static_cast<uint64_t>(other));
    }
  }
  uint64_t v;
};

// Implement generic numeric comparison against int value.
// Delegates to uint / double if either value is uint / double.
struct IntCompareVisitor {
  constexpr explicit IntCompareVisitor(int64_t v) : v(v) {}

  constexpr ComparisonResult operator()(double other) {
    return Invert(DoubleCompareVisitor(other)(v));
  }

  constexpr ComparisonResult operator()(uint64_t other) {
    return Invert(UintCompareVisitor(other)(v));
  }

  constexpr ComparisonResult operator()(int64_t other) {
    return Compare(v, other);
  }
  int64_t v;
};

struct CompareVisitor {
  explicit constexpr CompareVisitor(NumberVariant rhs) : rhs(rhs) {}

  CEL_ABSL_VISIT_CONSTEXPR ComparisonResult operator()(double v) {
    return absl::visit(DoubleCompareVisitor(v), rhs);
  }

  CEL_ABSL_VISIT_CONSTEXPR ComparisonResult operator()(uint64_t v) {
    return absl::visit(UintCompareVisitor(v), rhs);
  }

  CEL_ABSL_VISIT_CONSTEXPR ComparisonResult operator()(int64_t v) {
    return absl::visit(IntCompareVisitor(v), rhs);
  }
  NumberVariant rhs;
};

struct LosslessConvertibleToIntVisitor {
  constexpr bool operator()(double value) const {
    return value >= kDoubleToIntMin && value <= kMaxDoubleRepresentableAsInt &&
           value == static_cast<double>(static_cast<int64_t>(value));
  }
  constexpr bool operator()(uint64_t value) const {
    return value <= kUintToIntMax;
  }
  constexpr bool operator()(int64_t value) const { return true; }
};

struct LosslessConvertibleToUintVisitor {
  constexpr bool operator()(double value) const {
    return value >= 0 && value <= kMaxDoubleRepresentableAsUint &&
           value == static_cast<double>(static_cast<uint64_t>(value));
  }
  constexpr bool operator()(uint64_t value) const { return true; }
  constexpr bool operator()(int64_t value) const { return value >= 0; }
};

// Utility class for CEL number operations.
//
// In CEL expressions, comparisons between different numeric types are treated
// as all happening on the same continuous number line. This generally means
// that integers and doubles in convertible range are compared after converting
// to doubles (tolerating some loss of precision).
//
// This extends to key lookups -- {1: 'abc'}[1.0f] is expected to work since
// 1.0 == 1 in CEL.
class Number {
 public:
  // Factories to resolve ambiguous overload resolution against literals.
  static constexpr Number FromInt64(int64_t value) { return Number(value); }
  static constexpr Number FromUint64(uint64_t value) { return Number(value); }
  static constexpr Number FromDouble(double value) { return Number(value); }

  constexpr explicit Number(double double_value) : value_(double_value) {}
  constexpr explicit Number(int64_t int_value) : value_(int_value) {}
  constexpr explicit Number(uint64_t uint_value) : value_(uint_value) {}

  // Return a double representation of the value.
  CEL_ABSL_VISIT_CONSTEXPR double AsDouble() const {
    return absl::visit(internal::ConversionVisitor<double>(), value_);
  }

  // Return signed int64_t representation for the value.
  // Caller must guarantee the underlying value is representatble as an
  // int.
  CEL_ABSL_VISIT_CONSTEXPR int64_t AsInt() const {
    return absl::visit(internal::ConversionVisitor<int64_t>(), value_);
  }

  // Return unsigned int64_t representation for the value.
  // Caller must guarantee the underlying value is representable as an
  // uint.
  CEL_ABSL_VISIT_CONSTEXPR uint64_t AsUint() const {
    return absl::visit(internal::ConversionVisitor<uint64_t>(), value_);
  }

  // For key lookups, check if the conversion to signed int is lossless.
  CEL_ABSL_VISIT_CONSTEXPR bool LosslessConvertibleToInt() const {
    return absl::visit(internal::LosslessConvertibleToIntVisitor(), value_);
  }

  // For key lookups, check if the conversion to unsigned int is lossless.
  CEL_ABSL_VISIT_CONSTEXPR bool LosslessConvertibleToUint() const {
    return absl::visit(internal::LosslessConvertibleToUintVisitor(), value_);
  }

  CEL_ABSL_VISIT_CONSTEXPR bool operator<(Number other) const {
    return Compare(other) == internal::ComparisonResult::kLesser;
  }

  CEL_ABSL_VISIT_CONSTEXPR bool operator<=(Number other) const {
    internal::ComparisonResult cmp = Compare(other);
    return cmp != internal::ComparisonResult::kGreater &&
           cmp != internal::ComparisonResult::kNanInequal;
  }

  CEL_ABSL_VISIT_CONSTEXPR bool operator>(Number other) const {
    return Compare(other) == internal::ComparisonResult::kGreater;
  }

  CEL_ABSL_VISIT_CONSTEXPR bool operator>=(Number other) const {
    internal::ComparisonResult cmp = Compare(other);
    return cmp != internal::ComparisonResult::kLesser &&
           cmp != internal::ComparisonResult::kNanInequal;
  }

  CEL_ABSL_VISIT_CONSTEXPR bool operator==(Number other) const {
    return Compare(other) == internal::ComparisonResult::kEqual;
  }

  CEL_ABSL_VISIT_CONSTEXPR bool operator!=(Number other) const {
    return Compare(other) != internal::ComparisonResult::kEqual;
  }

  // Visit the underlying number representation, a variant of double, uint64_t,
  // or int64_t.
  template <typename T, typename Op>
  T visit(Op&& op) const {
    return absl::visit(std::forward<Op>(op), value_);
  }

 private:
  internal::NumberVariant value_;

  CEL_ABSL_VISIT_CONSTEXPR internal::ComparisonResult Compare(
      Number other) const {
    return absl::visit(internal::CompareVisitor(other.value_), value_);
  }
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_NUMBER_H_
