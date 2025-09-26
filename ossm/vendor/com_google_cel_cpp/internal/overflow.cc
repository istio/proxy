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

#include "internal/overflow.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "internal/status_macros.h"
#include "internal/time.h"

namespace cel::internal {
namespace {

constexpr int64_t kInt32Max = std::numeric_limits<int32_t>::max();
constexpr int64_t kInt32Min = std::numeric_limits<int32_t>::lowest();
constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
constexpr int64_t kInt64Min = std::numeric_limits<int64_t>::lowest();
constexpr uint64_t kUint32Max = std::numeric_limits<uint32_t>::max();
ABSL_ATTRIBUTE_UNUSED constexpr uint64_t kUint64Max =
    std::numeric_limits<uint64_t>::max();
constexpr uint64_t kUintToIntMax = static_cast<uint64_t>(kInt64Max);
constexpr double kDoubleToIntMax = static_cast<double>(kInt64Max);
constexpr double kDoubleToIntMin = static_cast<double>(kInt64Min);
const double kDoubleTwoTo64 = std::ldexp(1.0, 64);  // 1.0 * 2^64

const absl::Duration kOneSecondDuration = absl::Seconds(1);
const int64_t kOneSecondNanos = absl::ToInt64Nanoseconds(kOneSecondDuration);
// Number of seconds between `0001-01-01T00:00:00Z` and Unix epoch.
const int64_t kMinUnixTime =
    absl::ToInt64Seconds(MinTimestamp() - absl::UnixEpoch());

// Number of seconds between `9999-12-31T23:59:59.999999999Z` and Unix epoch.
const int64_t kMaxUnixTime =
    absl::ToInt64Seconds(MaxTimestamp() - absl::UnixEpoch());

absl::Status CheckRange(bool valid_expression,
                        absl::string_view error_message) {
  return valid_expression ? absl::OkStatus()
                          : absl::OutOfRangeError(error_message);
}

absl::Status CheckArgument(bool valid_expression,
                           absl::string_view error_message) {
  return valid_expression ? absl::OkStatus()
                          : absl::InvalidArgumentError(error_message);
}

// Determine whether the duration is finite.
bool IsFinite(absl::Duration d) {
  return d != absl::InfiniteDuration() && d != -absl::InfiniteDuration();
}

// Determine whether the time is finite.
bool IsFinite(absl::Time t) {
  return t != absl::InfiniteFuture() && t != absl::InfinitePast();
}

}  // namespace

absl::StatusOr<int64_t> CheckedAdd(int64_t x, int64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_add_overflow)
  int64_t sum;
  if (!__builtin_add_overflow(x, y, &sum)) {
    return sum;
  }
  return absl::OutOfRangeError("integer overflow");
#else
  CEL_RETURN_IF_ERROR(CheckRange(
      y > 0 ? x <= kInt64Max - y : x >= kInt64Min - y, "integer overflow"));
  return x + y;
#endif
}

absl::StatusOr<int64_t> CheckedSub(int64_t x, int64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_sub_overflow)
  int64_t diff;
  if (!__builtin_sub_overflow(x, y, &diff)) {
    return diff;
  }
  return absl::OutOfRangeError("integer overflow");
#else
  CEL_RETURN_IF_ERROR(CheckRange(
      y < 0 ? x <= kInt64Max + y : x >= kInt64Min + y, "integer overflow"));
  return x - y;
#endif
}

absl::StatusOr<int64_t> CheckedNegation(int64_t v) {
#if ABSL_HAVE_BUILTIN(__builtin_mul_overflow)
  int64_t prod;
  if (!__builtin_mul_overflow(v, -1, &prod)) {
    return prod;
  }
  return absl::OutOfRangeError("integer overflow");
#else
  CEL_RETURN_IF_ERROR(CheckRange(v != kInt64Min, "integer overflow"));
  return -v;
#endif
}

absl::StatusOr<int64_t> CheckedMul(int64_t x, int64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_mul_overflow)
  int64_t prod;
  if (!__builtin_mul_overflow(x, y, &prod)) {
    return prod;
  }
  return absl::OutOfRangeError("integer overflow");
#else
  CEL_RETURN_IF_ERROR(
      CheckRange(!((x == -1 && y == kInt64Min) || (y == -1 && x == kInt64Min) ||
                   (x > 0 && y > 0 && x > kInt64Max / y) ||
                   (x < 0 && y < 0 && x < kInt64Max / y) ||
                   // Avoid dividing kInt64Min by -1, use whichever value of x
                   // or y is positive as the divisor.
                   (x > 0 && y < 0 && y < kInt64Min / x) ||
                   (x < 0 && y > 0 && x < kInt64Min / y)),
                 "integer overflow"));
  return x * y;
#endif
}

absl::StatusOr<int64_t> CheckedDiv(int64_t x, int64_t y) {
  CEL_RETURN_IF_ERROR(
      CheckRange(x != kInt64Min || y != -1, "integer overflow"));
  CEL_RETURN_IF_ERROR(CheckArgument(y != 0, "divide by zero"));
  return x / y;
}

absl::StatusOr<int64_t> CheckedMod(int64_t x, int64_t y) {
  CEL_RETURN_IF_ERROR(
      CheckRange(x != kInt64Min || y != -1, "integer overflow"));
  CEL_RETURN_IF_ERROR(CheckArgument(y != 0, "modulus by zero"));
  return x % y;
}

absl::StatusOr<uint64_t> CheckedAdd(uint64_t x, uint64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_add_overflow)
  uint64_t sum;
  if (!__builtin_add_overflow(x, y, &sum)) {
    return sum;
  }
  return absl::OutOfRangeError("unsigned integer overflow");
#else
  CEL_RETURN_IF_ERROR(
      CheckRange(x <= kUint64Max - y, "unsigned integer overflow"));
  return x + y;
#endif
}

absl::StatusOr<uint64_t> CheckedSub(uint64_t x, uint64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_sub_overflow)
  uint64_t diff;
  if (!__builtin_sub_overflow(x, y, &diff)) {
    return diff;
  }
  return absl::OutOfRangeError("unsigned integer overflow");
#else
  CEL_RETURN_IF_ERROR(CheckRange(y <= x, "unsigned integer overflow"));
  return x - y;
#endif
}

absl::StatusOr<uint64_t> CheckedMul(uint64_t x, uint64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_mul_overflow)
  uint64_t prod;
  if (!__builtin_mul_overflow(x, y, &prod)) {
    return prod;
  }
  return absl::OutOfRangeError("unsigned integer overflow");
#else
  CEL_RETURN_IF_ERROR(
      CheckRange(y == 0 || x <= kUint64Max / y, "unsigned integer overflow"));
  return x * y;
#endif
}

absl::StatusOr<uint64_t> CheckedDiv(uint64_t x, uint64_t y) {
  CEL_RETURN_IF_ERROR(CheckArgument(y != 0, "divide by zero"));
  return x / y;
}

absl::StatusOr<uint64_t> CheckedMod(uint64_t x, uint64_t y) {
  CEL_RETURN_IF_ERROR(CheckArgument(y != 0, "modulus by zero"));
  return x % y;
}

absl::StatusOr<absl::Duration> CheckedAdd(absl::Duration x, absl::Duration y) {
  CEL_RETURN_IF_ERROR(
      CheckRange(IsFinite(x) && IsFinite(y), "integer overflow"));
  // absl::Duration can handle +- infinite durations, but the Go time.Duration
  // implementation caps the durations to those expressible within a single
  // int64 rather than (seconds int64, nanos int32).
  //
  // The absl implementation mirrors the protobuf implementation which supports
  // durations on the order of +- 10,000 years, but Go only supports +- 290 year
  // durations.
  //
  // Since Go is the more conservative of the implementations and 290 year
  // durations seem quite reasonable, this code mirrors the conservative
  // overflow behavior which would be observed in Go.
  CEL_ASSIGN_OR_RETURN(int64_t nanos, CheckedAdd(absl::ToInt64Nanoseconds(x),
                                                 absl::ToInt64Nanoseconds(y)));
  return absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Duration> CheckedSub(absl::Duration x, absl::Duration y) {
  CEL_RETURN_IF_ERROR(
      CheckRange(IsFinite(x) && IsFinite(y), "integer overflow"));
  CEL_ASSIGN_OR_RETURN(int64_t nanos, CheckedSub(absl::ToInt64Nanoseconds(x),
                                                 absl::ToInt64Nanoseconds(y)));
  return absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Duration> CheckedNegation(absl::Duration v) {
  CEL_RETURN_IF_ERROR(CheckRange(IsFinite(v), "integer overflow"));
  CEL_ASSIGN_OR_RETURN(int64_t nanos,
                       CheckedNegation(absl::ToInt64Nanoseconds(v)));
  return absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Time> CheckedAdd(absl::Time t, absl::Duration d) {
  CEL_RETURN_IF_ERROR(
      CheckRange(IsFinite(t) && IsFinite(d), "timestamp overflow"));
  // First we break time into its components by truncating and subtracting.
  const int64_t s1 = absl::ToUnixSeconds(t);
  const int64_t ns1 = (t - absl::FromUnixSeconds(s1)) / absl::Nanoseconds(1);

  // Second we break duration into its components by dividing and modulo.
  // Truncate to seconds.
  const int64_t s2 = d / kOneSecondDuration;
  // Get remainder.
  const int64_t ns2 = absl::ToInt64Nanoseconds(d % kOneSecondDuration);

  // Add seconds first, detecting any overflow.
  CEL_ASSIGN_OR_RETURN(int64_t s, CheckedAdd(s1, s2));
  // Nanoseconds cannot overflow as nanos are normalized to [0, 999999999].
  absl::Duration ns = absl::Nanoseconds(ns2 + ns1);

  // Normalize nanoseconds to be positive and carry extra nanos to seconds.
  if (ns < absl::ZeroDuration() || ns >= kOneSecondDuration) {
    // Add seconds, or no-op if nanseconds negative (ns never < -999_999_999ns)
    CEL_ASSIGN_OR_RETURN(s, CheckedAdd(s, ns / kOneSecondDuration));
    ns -= (ns / kOneSecondDuration) * kOneSecondDuration;
    // Subtract a second to make the nanos positive.
    if (ns < absl::ZeroDuration()) {
      CEL_ASSIGN_OR_RETURN(s, CheckedAdd(s, -1));
      ns += kOneSecondDuration;
    }
  }
  // Check if the the number of seconds from Unix epoch is within our acceptable
  // range.
  CEL_RETURN_IF_ERROR(
      CheckRange(s >= kMinUnixTime && s <= kMaxUnixTime, "timestamp overflow"));

  // Return resulting time.
  return absl::FromUnixSeconds(s) + ns;
}

absl::StatusOr<absl::Time> CheckedSub(absl::Time t, absl::Duration d) {
  CEL_ASSIGN_OR_RETURN(auto neg_duration, CheckedNegation(d));
  return CheckedAdd(t, neg_duration);
}

absl::StatusOr<absl::Duration> CheckedSub(absl::Time t1, absl::Time t2) {
  CEL_RETURN_IF_ERROR(
      CheckRange(IsFinite(t1) && IsFinite(t2), "integer overflow"));
  // First we break time into its components by truncating and subtracting.
  const int64_t s1 = absl::ToUnixSeconds(t1);
  const int64_t ns1 = (t1 - absl::FromUnixSeconds(s1)) / absl::Nanoseconds(1);
  const int64_t s2 = absl::ToUnixSeconds(t2);
  const int64_t ns2 = (t2 - absl::FromUnixSeconds(s2)) / absl::Nanoseconds(1);

  // Subtract seconds first, detecting any overflow.
  CEL_ASSIGN_OR_RETURN(int64_t s, CheckedSub(s1, s2));
  // Nanoseconds cannot overflow as nanos are normalized to [0, 999999999].
  absl::Duration ns = absl::Nanoseconds(ns1 - ns2);

  // Scale the seconds result to nanos.
  CEL_ASSIGN_OR_RETURN(const int64_t t, CheckedMul(s, kOneSecondNanos));
  // Add the seconds (scaled to nanos) to the nanosecond value.
  CEL_ASSIGN_OR_RETURN(const int64_t v,
                       CheckedAdd(t, absl::ToInt64Nanoseconds(ns)));
  return absl::Nanoseconds(v);
}

absl::StatusOr<int64_t> CheckedDoubleToInt64(double v) {
  CEL_RETURN_IF_ERROR(
      CheckRange(std::isfinite(v) && v < kDoubleToIntMax && v > kDoubleToIntMin,
                 "double out of int64 range"));
  return static_cast<int64_t>(v);
}

absl::StatusOr<uint64_t> CheckedDoubleToUint64(double v) {
  CEL_RETURN_IF_ERROR(
      CheckRange(std::isfinite(v) && v >= 0 && v < kDoubleTwoTo64,
                 "double out of uint64 range"));
  return static_cast<uint64_t>(v);
}

absl::StatusOr<uint64_t> CheckedInt64ToUint64(int64_t v) {
  CEL_RETURN_IF_ERROR(CheckRange(v >= 0, "int64 out of uint64 range"));
  return static_cast<uint64_t>(v);
}

absl::StatusOr<int32_t> CheckedInt64ToInt32(int64_t v) {
  CEL_RETURN_IF_ERROR(
      CheckRange(v >= kInt32Min && v <= kInt32Max, "int64 out of int32 range"));
  return static_cast<int32_t>(v);
}

absl::StatusOr<int64_t> CheckedUint64ToInt64(uint64_t v) {
  CEL_RETURN_IF_ERROR(
      CheckRange(v <= kUintToIntMax, "uint64 out of int64 range"));
  return static_cast<int64_t>(v);
}

absl::StatusOr<uint32_t> CheckedUint64ToUint32(uint64_t v) {
  CEL_RETURN_IF_ERROR(
      CheckRange(v <= kUint32Max, "uint64 out of uint32 range"));
  return static_cast<uint32_t>(v);
}

}  // namespace cel::internal
