// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_OVERFLOW_H_
#define THIRD_PARTY_CEL_CPP_COMMON_OVERFLOW_H_

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace cel::internal {

// Add two int64_t values together.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   int64_t_max + 1
absl::StatusOr<int64_t> CheckedAdd(int64_t x, int64_t y);

// Subtract two int64_t values from each other.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError. e.g.
//   int64_t_min - 1
absl::StatusOr<int64_t> CheckedSub(int64_t x, int64_t y);

// Negate an int64_t value.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   negate(int64_t_min)
absl::StatusOr<int64_t> CheckedNegation(int64_t v);

// Multiply two int64_t values together.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError. e.g.
//   2 * int64_t_max
absl::StatusOr<int64_t> CheckedMul(int64_t x, int64_t y);

// Divide one int64_t value into another.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   int64_t_min / -1
absl::StatusOr<int64_t> CheckedDiv(int64_t x, int64_t y);

// Compute the modulus of x into y.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   int64_t_min % -1
absl::StatusOr<int64_t> CheckedMod(int64_t x, int64_t y);

// Add two uint64_t values together.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   uint64_t_max + 1
absl::StatusOr<uint64_t> CheckedAdd(uint64_t x, uint64_t y);

// Subtract two uint64_t values from each other.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   1 - uint64_t_max
absl::StatusOr<uint64_t> CheckedSub(uint64_t x, uint64_t y);

// Multiply two uint64_t values together.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   2 * uint64_t_max
absl::StatusOr<uint64_t> CheckedMul(uint64_t x, uint64_t y);

// Divide one uint64_t value into another.
absl::StatusOr<uint64_t> CheckedDiv(uint64_t x, uint64_t y);

// Compute the modulus of x into y.
// If 'y' is zero, the function will return an
// absl::StatusCode::kInvalidArgumentError, e.g. 1 / 0.
absl::StatusOr<uint64_t> CheckedMod(uint64_t x, uint64_t y);

// Add two durations together.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   duration(int64_t_max, "ns") + duration(int64_t_max, "ns")
//
// Note, absl::Duration is effectively an int64_t under the covers, which means
// the same cases that would result in overflow for int64_t values would hold
// true for absl::Duration values.
absl::StatusOr<absl::Duration> CheckedAdd(absl::Duration x, absl::Duration y);

// Subtract two durations from each other.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   duration(int64_t_min, "ns") - duration(1, "ns")
//
// Note, absl::Duration is effectively an int64_t under the covers, which means
// the same cases that would result in overflow for int64_t values would hold
// true for absl::Duration values.
absl::StatusOr<absl::Duration> CheckedSub(absl::Duration x, absl::Duration y);

// Negate a duration.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   negate(duration(int64_t_min, "ns")).
absl::StatusOr<absl::Duration> CheckedNegation(absl::Duration v);

// Add an absl::Time and absl::Duration value together.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   timestamp(unix_epoch_max) + duration(1, "ns")
//
// Valid time values must be between `0001-01-01T00:00:00Z` (-62135596800s) and
// `9999-12-31T23:59:59.999999999Z` (253402300799s).
absl::StatusOr<absl::Time> CheckedAdd(absl::Time t, absl::Duration d);

// Subtract an absl::Time and absl::Duration value together.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   timestamp(unix_epoch_min) - duration(1, "ns")
//
// Valid time values must be between `0001-01-01T00:00:00Z` (-62135596800s) and
// `9999-12-31T23:59:59.999999999Z` (253402300799s).
absl::StatusOr<absl::Time> CheckedSub(absl::Time t, absl::Duration d);

// Subtract two absl::Time values from each other to produce an absl::Duration.
// If overflow is detected, return an absl::StatusCode::kOutOfRangeError, e.g.
//   timestamp(unix_epoch_min) - timestamp(unix_epoch_max)
absl::StatusOr<absl::Duration> CheckedSub(absl::Time t1, absl::Time t2);

// Convert a double value to an int64_t if possible.
// If the double exceeds the values representable in an int64_t the function
// will return an absl::StatusCode::kOutOfRangeError.
//
// Only finite double values may be converted to an int64_t. CEL may also reject
// some conversions if the value falls into a range where overflow would be
// ambiguous.
//
// The behavior of the static_cast<int64_t_t>(double) assembly instruction on
// x86 (cvttsd2si) can be manipulated by the <cfenv> header:
// https://en.cppreference.com/w/cpp/numeric/fenv/feround. This means that the
// set of values which will result in a valid or invalid conversion are
// environment dependent and the implementation must err on the side of caution
// and reject possibly valid values which might be invalid based on environment
// settings.
absl::StatusOr<int64_t> CheckedDoubleToInt64(double v);

// Convert a double value to a uint64_t if possible.
// If the double exceeds the values representable in a uint64_t the function
// will return an absl::StatusCode::kOutOfRangeError.
//
// Only finite double values may be converted to a uint64_t. CEL may also reject
// some conversions if the value falls into a range where overflow would be
// ambiguous.
//
// The behavior of the static_cast<uint64_t_t>(double) assembly instruction on
// x86 (cvttsd2si) can be manipulated by the <cfenv> header:
// https://en.cppreference.com/w/cpp/numeric/fenv/feround. This means that the
// set of values which will result in a valid or invalid conversion are
// environment dependent and the implementation must err on the side of caution
// and reject possibly valid values which might be invalid based on environment
// settings.
absl::StatusOr<uint64_t> CheckedDoubleToUint64(double v);

// Convert an int64_t value to a uint64_t value if possible.
// If the int64_t exceeds the values representable in a uint64_t the function
// will return an absl::StatusCode::kOutOfRangeError.
absl::StatusOr<uint64_t> CheckedInt64ToUint64(int64_t v);

// Convert an int64_t value to an int32_t value if possible.
// If the int64_t exceeds the values representable in an int32_t the function
// will return an absl::StatusCode::kOutOfRangeError.
absl::StatusOr<int32_t> CheckedInt64ToInt32(int64_t v);

// Convert a uint64_t value to an int64_t value if possible.
// If the uint64_t exceeds the values representable in an int64_t the function
// will return an absl::StatusCode::kOutOfRangeError.
absl::StatusOr<int64_t> CheckedUint64ToInt64(uint64_t v);

// Convert a uint64_t value to a uint32_t value if possible.
// If the uint64_t exceeds the values representable in a uint32_t the function
// will return an absl::StatusCode::kOutOfRangeError.
absl::StatusOr<uint32_t> CheckedUint64ToUint32(uint64_t v);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_OVERFLOW_H_
