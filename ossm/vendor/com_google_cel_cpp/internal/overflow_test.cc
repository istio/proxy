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

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

using ::testing::HasSubstr;
using ::testing::ValuesIn;

template <typename T>
struct TestCase {
  std::string test_name;
  absl::FunctionRef<absl::StatusOr<T>()> op;
  absl::StatusOr<T> result;
};

template <typename T>
void ExpectResult(const T& test_case) {
  auto result = test_case.op();
  ASSERT_EQ(result.status().code(), test_case.result.status().code());
  if (result.ok()) {
    EXPECT_EQ(*result, *test_case.result);
  } else {
    EXPECT_THAT(result.status().message(),
                HasSubstr(test_case.result.status().message()));
  }
}

using IntTestCase = TestCase<int64_t>;
using CheckedIntResultTest = testing::TestWithParam<IntTestCase>;
TEST_P(CheckedIntResultTest, IntOperations) { ExpectResult(GetParam()); }

INSTANTIATE_TEST_SUITE_P(
    CheckedIntMathTest, CheckedIntResultTest,
    ValuesIn(std::vector<IntTestCase>{
        // Addition tests.
        {"OneAddOne", [] { return CheckedAdd(1L, 1L); }, 2L},
        {"ZeroAddOne", [] { return CheckedAdd(0, 1L); }, 1L},
        {"ZeroAddMinusOne", [] { return CheckedAdd(0, -1L); }, -1L},
        {"OneAddZero", [] { return CheckedAdd(1L, 0); }, 1L},
        {"MinusOneAddZero", [] { return CheckedAdd(-1L, 0); }, -1L},
        {"OneAddIntMax",
         [] { return CheckedAdd(1L, std::numeric_limits<int64_t>::max()); },
         absl::OutOfRangeError("integer overflow")},
        {"MinusOneAddIntMin",
         [] { return CheckedAdd(-1L, std::numeric_limits<int64_t>::lowest()); },
         absl::OutOfRangeError("integer overflow")},

        // Subtraction tests.
        {"TwoSubThree", [] { return CheckedSub(2L, 3L); }, -1L},
        {"TwoSubZero", [] { return CheckedSub(2L, 0); }, 2L},
        {"ZeroSubTwo", [] { return CheckedSub(0, 2L); }, -2L},
        {"MinusTwoSubThree", [] { return CheckedSub(-2L, 3L); }, -5L},
        {"MinusTwoSubZero", [] { return CheckedSub(-2L, 0); }, -2L},
        {"ZeroSubMinusTwo", [] { return CheckedSub(0, -2L); }, 2L},
        {"IntMinSubIntMax",
         [] {
           return CheckedSub(std::numeric_limits<int64_t>::max(),
                             std::numeric_limits<int64_t>::lowest());
         },
         absl::OutOfRangeError("integer overflow")},

        // Multiplication tests.
        {"TwoMulThree", [] { return CheckedMul(2L, 3L); }, 6L},
        {"MinusTwoMulThree", [] { return CheckedMul(-2L, 3L); }, -6L},
        {"MinusTwoMulMinusThree", [] { return CheckedMul(-2L, -3L); }, 6L},
        {"TwoMulMinusThree", [] { return CheckedMul(2L, -3L); }, -6L},
        {"TwoMulIntMax",
         [] { return CheckedMul(2L, std::numeric_limits<int64_t>::max()); },
         absl::OutOfRangeError("integer overflow")},
        {"MinusOneMulIntMin",
         [] { return CheckedMul(-1L, std::numeric_limits<int64_t>::lowest()); },
         absl::OutOfRangeError("integer overflow")},
        {"IntMinMulMinusOne",
         [] { return CheckedMul(std::numeric_limits<int64_t>::lowest(), -1L); },
         absl::OutOfRangeError("integer overflow")},
        {"IntMinMulZero",
         [] { return CheckedMul(std::numeric_limits<int64_t>::lowest(), 0); },
         0},
        {"ZeroMulIntMin",
         [] { return CheckedMul(0, std::numeric_limits<int64_t>::lowest()); },
         0},
        {"IntMaxMulZero",
         [] { return CheckedMul(std::numeric_limits<int64_t>::max(), 0); }, 0},
        {"ZeroMulIntMax",
         [] { return CheckedMul(0, std::numeric_limits<int64_t>::max()); }, 0},

        // Division cases.
        {"ZeroDivOne", [] { return CheckedDiv(0, 1L); }, 0},
        {"TenDivTwo", [] { return CheckedDiv(10L, 2L); }, 5},
        {"TenDivMinusOne", [] { return CheckedDiv(10L, -1L); }, -10},
        {"MinusTenDivMinusOne", [] { return CheckedDiv(-10L, -1L); }, 10},
        {"MinusTenDivTwo", [] { return CheckedDiv(-10L, 2L); }, -5},
        {"OneDivZero", [] { return CheckedDiv(1L, 0L); },
         absl::InvalidArgumentError("divide by zero")},
        {"IntMinDivMinusOne",
         [] { return CheckedDiv(std::numeric_limits<int64_t>::lowest(), -1L); },
         absl::OutOfRangeError("integer overflow")},

        // Modulus cases.
        {"ZeroModTwo", [] { return CheckedMod(0, 2L); }, 0},
        {"TwoModTwo", [] { return CheckedMod(2L, 2L); }, 0},
        {"ThreeModTwo", [] { return CheckedMod(3L, 2L); }, 1L},
        {"TwoModZero", [] { return CheckedMod(2L, 0); },
         absl::InvalidArgumentError("modulus by zero")},
        {"IntMinModTwo",
         [] { return CheckedMod(std::numeric_limits<int64_t>::lowest(), 2L); },
         0},
        {"IntMaxModMinusOne",
         [] { return CheckedMod(std::numeric_limits<int64_t>::max(), -1L); },
         0},
        {"IntMinModMinusOne",
         [] { return CheckedMod(std::numeric_limits<int64_t>::lowest(), -1L); },
         absl::OutOfRangeError("integer overflow")},

        // Negation cases.
        {"NegateOne", [] { return CheckedNegation(1L); }, -1L},
        {"NegateMinInt64",
         [] { return CheckedNegation(std::numeric_limits<int64_t>::lowest()); },
         absl::OutOfRangeError("integer overflow")},

        // Numeric conversion cases for uint -> int, double -> int
        {"Uint64Conversion", [] { return CheckedUint64ToInt64(1UL); }, 1L},
        {"Uint32MaxConversion",
         [] {
           return CheckedUint64ToInt64(
               static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
         },
         std::numeric_limits<int64_t>::max()},
        {"Uint32MaxConversionError",
         [] {
           return CheckedUint64ToInt64(
               static_cast<uint64_t>(std::numeric_limits<uint64_t>::max()));
         },
         absl::OutOfRangeError("out of int64 range")},
        {"DoubleConversion", [] { return CheckedDoubleToInt64(100.1); }, 100L},
        {"DoubleInt64MaxConversionError",
         [] {
           return CheckedDoubleToInt64(
               static_cast<double>(std::numeric_limits<int64_t>::max()));
         },
         absl::OutOfRangeError("out of int64 range")},
        {"DoubleInt64MaxMinus512Conversion",
         [] {
           return CheckedDoubleToInt64(
               static_cast<double>(std::numeric_limits<int64_t>::max() - 512));
         },
         std::numeric_limits<int64_t>::max() - 1023},
        {"DoubleInt64MaxMinus1024Conversion",
         [] {
           return CheckedDoubleToInt64(
               static_cast<double>(std::numeric_limits<int64_t>::max() - 1024));
         },
         std::numeric_limits<int64_t>::max() - 1023},
        {"DoubleInt64MinConversionError",
         [] {
           return CheckedDoubleToInt64(
               static_cast<double>(std::numeric_limits<int64_t>::lowest()));
         },
         absl::OutOfRangeError("out of int64 range")},
        {"DoubleInt64MinMinusOneConversionError",
         [] {
           return CheckedDoubleToInt64(
               static_cast<double>(std::numeric_limits<int64_t>::lowest()) -
               1.0);
         },
         absl::OutOfRangeError("out of int64 range")},
        {"DoubleInt64MinMinus511ConversionError",
         [] {
           return CheckedDoubleToInt64(
               static_cast<double>(std::numeric_limits<int64_t>::lowest()) -
               511.0);
         },
         absl::OutOfRangeError("out of int64 range")},
        {"InfiniteConversionError",
         [] {
           return CheckedDoubleToInt64(std::numeric_limits<double>::infinity());
         },
         absl::OutOfRangeError("out of int64 range")},
        {"NegRangeConversionError",
         [] { return CheckedDoubleToInt64(-1.0e99); },
         absl::OutOfRangeError("out of int64 range")},
        {"PosRangeConversionError", [] { return CheckedDoubleToInt64(1.0e99); },
         absl::OutOfRangeError("out of int64 range")},
    }),
    [](const testing::TestParamInfo<CheckedIntResultTest::ParamType>& info) {
      return info.param.test_name;
    });

using UintTestCase = TestCase<uint64_t>;
using CheckedUintResultTest = testing::TestWithParam<UintTestCase>;
TEST_P(CheckedUintResultTest, UnsignedOperations) { ExpectResult(GetParam()); }

INSTANTIATE_TEST_SUITE_P(
    CheckedUintMathTest, CheckedUintResultTest,
    ValuesIn(std::vector<UintTestCase>{
        // Addition tests.
        {"OneAddOne", [] { return CheckedAdd(1UL, 1UL); }, 2UL},
        {"ZeroAddOne", [] { return CheckedAdd(0, 1UL); }, 1UL},
        {"OneAddZero", [] { return CheckedAdd(1UL, 0); }, 1UL},
        {"OneAddIntMax",
         [] { return CheckedAdd(1UL, std::numeric_limits<uint64_t>::max()); },
         absl::OutOfRangeError("unsigned integer overflow")},

        // Subtraction tests.
        {"OneSubOne", [] { return CheckedSub(1UL, 1UL); }, 0},
        {"ZeroSubOne", [] { return CheckedSub(0, 1UL); },
         absl::OutOfRangeError("unsigned integer overflow")},
        {"OneSubZero", [] { return CheckedSub(1UL, 0); }, 1UL},

        // Multiplication tests.
        {"OneMulOne", [] { return CheckedMul(1UL, 1UL); }, 1UL},
        {"ZeroMulOne", [] { return CheckedMul(0, 1UL); }, 0},
        {"OneMulZero", [] { return CheckedMul(1UL, 0); }, 0},
        {"TwoMulUintMax",
         [] { return CheckedMul(2UL, std::numeric_limits<uint64_t>::max()); },
         absl::OutOfRangeError("unsigned integer overflow")},

        // Division tests.
        {"TwoDivTwo", [] { return CheckedDiv(2UL, 2UL); }, 1UL},
        {"TwoDivFour", [] { return CheckedDiv(2UL, 4UL); }, 0},
        {"OneDivZero", [] { return CheckedDiv(1UL, 0); },
         absl::InvalidArgumentError("divide by zero")},

        // Modulus tests.
        {"TwoModTwo", [] { return CheckedMod(2UL, 2UL); }, 0},
        {"TwoModFour", [] { return CheckedMod(2UL, 4UL); }, 2UL},
        {"OneModZero", [] { return CheckedMod(1UL, 0); },
         absl::InvalidArgumentError("modulus by zero")},

        // Conversion test cases for int -> uint, double -> uint.
        {"Int64Conversion", [] { return CheckedInt64ToUint64(1L); }, 1UL},
        {"Int64MaxConversion",
         [] {
           return CheckedInt64ToUint64(std::numeric_limits<int64_t>::max());
         },
         static_cast<uint64_t>(std::numeric_limits<int64_t>::max())},
        {"NegativeInt64ConversionError",
         [] { return CheckedInt64ToUint64(-1L); },
         absl::OutOfRangeError("out of uint64 range")},
        {"DoubleConversion", [] { return CheckedDoubleToUint64(100.1); },
         100UL},
        {"DoubleUint64MaxConversionError",
         [] {
           return CheckedDoubleToUint64(
               static_cast<double>(std::numeric_limits<uint64_t>::max()));
         },
         absl::OutOfRangeError("out of uint64 range")},
        {"DoubleUint64MaxMinus512Conversion",
         [] {
           return CheckedDoubleToUint64(
               static_cast<double>(std::numeric_limits<uint64_t>::max() - 512));
         },
         absl::OutOfRangeError("out of uint64 range")},
        {"DoubleUint64MaxMinus1024Conversion",
         [] {
           return CheckedDoubleToUint64(static_cast<double>(
               std::numeric_limits<uint64_t>::max() - 1024));
         },
         std::numeric_limits<uint64_t>::max() - 2047},
        {"InfiniteConversionError",
         [] {
           return CheckedDoubleToUint64(
               std::numeric_limits<double>::infinity());
         },
         absl::OutOfRangeError("out of uint64 range")},
        {"NegConversionError", [] { return CheckedDoubleToUint64(-1.1); },
         absl::OutOfRangeError("out of uint64 range")},
        {"NegRangeConversionError",
         [] { return CheckedDoubleToUint64(-1.0e99); },
         absl::OutOfRangeError("out of uint64 range")},
        {"PosRangeConversionError",
         [] { return CheckedDoubleToUint64(1.0e99); },
         absl::OutOfRangeError("out of uint64 range")},
    }),
    [](const testing::TestParamInfo<CheckedUintResultTest::ParamType>& info) {
      return info.param.test_name;
    });

using DurationTestCase = TestCase<absl::Duration>;
using CheckedDurationResultTest = testing::TestWithParam<DurationTestCase>;
TEST_P(CheckedDurationResultTest, DurationOperations) {
  ExpectResult(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    CheckedDurationMathTest, CheckedDurationResultTest,
    ValuesIn(std::vector<DurationTestCase>{
        // Addition tests.
        {"OneSecondAddOneSecond",
         [] { return CheckedAdd(absl::Seconds(1), absl::Seconds(1)); },
         absl::Seconds(2)},
        {"MaxDurationAddOneNano",
         [] {
           return CheckedAdd(
               absl::Nanoseconds(std::numeric_limits<int64_t>::max()),
               absl::Nanoseconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"MinDurationAddMinusOneNano",
         [] {
           return CheckedAdd(
               absl::Nanoseconds(std::numeric_limits<int64_t>::lowest()),
               absl::Nanoseconds(-1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"InfinityAddOneNano",
         [] {
           return CheckedAdd(absl::InfiniteDuration(), absl::Nanoseconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"NegInfinityAddOneNano",
         [] {
           return CheckedAdd(-absl::InfiniteDuration(), absl::Nanoseconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"OneSecondAddInfinity",
         [] {
           return CheckedAdd(absl::Nanoseconds(1), absl::InfiniteDuration());
         },
         absl::OutOfRangeError("integer overflow")},
        {"OneSecondAddNegInfinity",
         [] {
           return CheckedAdd(absl::Nanoseconds(1), -absl::InfiniteDuration());
         },
         absl::OutOfRangeError("integer overflow")},

        // Subtraction tests for duration - duration.
        {"OneSecondSubOneSecond",
         [] { return CheckedSub(absl::Seconds(1), absl::Seconds(1)); },
         absl::ZeroDuration()},
        {"MinDurationSubOneSecond",
         [] {
           return CheckedSub(
               absl::Nanoseconds(std::numeric_limits<int64_t>::lowest()),
               absl::Nanoseconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"InfinitySubOneNano",
         [] {
           return CheckedSub(absl::InfiniteDuration(), absl::Nanoseconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"NegInfinitySubOneNano",
         [] {
           return CheckedSub(-absl::InfiniteDuration(), absl::Nanoseconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"OneNanoSubInfinity",
         [] {
           return CheckedSub(absl::Nanoseconds(1), absl::InfiniteDuration());
         },
         absl::OutOfRangeError("integer overflow")},
        {"OneNanoSubNegInfinity",
         [] {
           return CheckedSub(absl::Nanoseconds(1), -absl::InfiniteDuration());
         },
         absl::OutOfRangeError("integer overflow")},

        // Subtraction tests for time - time.
        {"TimeSubOneSecond",
         [] {
           return CheckedSub(absl::FromUnixSeconds(100),
                             absl::FromUnixSeconds(1));
         },
         absl::Seconds(99)},
        {"TimeWithNanosPositive",
         [] {
           return CheckedSub(absl::FromUnixSeconds(2) + absl::Nanoseconds(1),
                             absl::FromUnixSeconds(1) - absl::Nanoseconds(1));
         },
         absl::Seconds(1) + absl::Nanoseconds(2)},
        {"TimeWithNanosNegative",
         [] {
           return CheckedSub(absl::FromUnixSeconds(1) + absl::Nanoseconds(1),
                             absl::FromUnixSeconds(2) + absl::Seconds(1) -
                                 absl::Nanoseconds(1));
         },
         absl::Seconds(-2) + absl::Nanoseconds(2)},
        {"MinTimestampMinusOne",
         [] {
           return CheckedSub(
               absl::FromUnixSeconds(std::numeric_limits<int64_t>::lowest()),
               absl::FromUnixSeconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"InfinitePastSubOneSecond",
         [] {
           return CheckedSub(absl::InfinitePast(), absl::FromUnixSeconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"InfiniteFutureSubOneMinusSecond",
         [] {
           return CheckedSub(absl::InfiniteFuture(), absl::FromUnixSeconds(-1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"InfiniteFutureSubInfinitePast",
         [] {
           return CheckedSub(absl::InfiniteFuture(), absl::InfinitePast());
         },
         absl::OutOfRangeError("integer overflow")},
        {"InfinitePastSubInfiniteFuture",
         [] {
           return CheckedSub(absl::InfinitePast(), absl::InfiniteFuture());
         },
         absl::OutOfRangeError("integer overflow")},

        // Negation cases.
        {"NegateOneSecond", [] { return CheckedNegation(absl::Seconds(1)); },
         absl::Seconds(-1)},
        {"NegateMinDuration",
         [] {
           return CheckedNegation(
               absl::Nanoseconds(std::numeric_limits<int64_t>::lowest()));
         },
         absl::OutOfRangeError("integer overflow")},
        {"NegateInfiniteDuration",
         [] { return CheckedNegation(absl::InfiniteDuration()); },
         absl::OutOfRangeError("integer overflow")},
        {"NegateNegInfiniteDuration",
         [] { return CheckedNegation(-absl::InfiniteDuration()); },
         absl::OutOfRangeError("integer overflow")},
    }),
    [](const testing::TestParamInfo<CheckedDurationResultTest::ParamType>&
           info) { return info.param.test_name; });

using TimeTestCase = TestCase<absl::Time>;
using CheckedTimeResultTest = testing::TestWithParam<TimeTestCase>;
TEST_P(CheckedTimeResultTest, TimeDurationOperations) {
  ExpectResult(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    CheckedTimeDurationMathTest, CheckedTimeResultTest,
    ValuesIn(std::vector<TimeTestCase>{
        // Addition tests.
        {"DateAddOneHourMinusOneMilli",
         [] {
           return CheckedAdd(absl::FromUnixSeconds(3506),
                             absl::Hours(1) + absl::Milliseconds(-1));
         },
         absl::FromUnixSeconds(7106) + absl::Milliseconds(-1)},
        {"DateAddOneHourOneNano",
         [] {
           return CheckedAdd(absl::FromUnixSeconds(3506),
                             absl::Hours(1) + absl::Nanoseconds(1));
         },
         absl::FromUnixSeconds(7106) + absl::Nanoseconds(1)},
        {"MaxIntAddOneSecond",
         [] {
           return CheckedAdd(
               absl::FromUnixSeconds(std::numeric_limits<int64_t>::max()),
               absl::Seconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"MaxTimestampAddOneSecond",
         [] {
           return CheckedAdd(absl::FromUnixSeconds(253402300799),
                             absl::Seconds(1));
         },
         absl::OutOfRangeError("timestamp overflow")},
        {"TimeWithNanosNegative",
         [] {
           return CheckedAdd(absl::FromUnixSeconds(1) + absl::Nanoseconds(1),
                             absl::Nanoseconds(-999999999));
         },
         absl::FromUnixNanos(2)},
        {"TimeWithNanosPositive",
         [] {
           return CheckedAdd(
               absl::FromUnixSeconds(1) + absl::Nanoseconds(999999999),
               absl::Nanoseconds(999999999));
         },
         absl::FromUnixSeconds(2) + absl::Nanoseconds(999999998)},
        {"SecondsAddInfinity",
         [] {
           return CheckedAdd(
               absl::FromUnixSeconds(1) + absl::Nanoseconds(999999999),
               absl::InfiniteDuration());
         },
         absl::OutOfRangeError("timestamp overflow")},
        {"SecondsAddNegativeInfinity",
         [] {
           return CheckedAdd(
               absl::FromUnixSeconds(1) + absl::Nanoseconds(999999999),
               -absl::InfiniteDuration());
         },
         absl::OutOfRangeError("timestamp overflow")},
        {"InfiniteFutureAddNegativeInfinity",
         [] {
           return CheckedAdd(absl::InfiniteFuture(), -absl::InfiniteDuration());
         },
         absl::OutOfRangeError("timestamp overflow")},
        {"InfinitePastAddInfinity",
         [] {
           return CheckedAdd(absl::InfinitePast(), absl::InfiniteDuration());
         },
         absl::OutOfRangeError("timestamp overflow")},

        // Subtraction tests.
        {"DateSubOneHour",
         [] { return CheckedSub(absl::FromUnixSeconds(3506), absl::Hours(1)); },
         absl::FromUnixSeconds(-94)},
        {"MinTimestampSubOneSecond",
         [] {
           return CheckedSub(absl::FromUnixSeconds(-62135596800),
                             absl::Seconds(1));
         },
         absl::OutOfRangeError("timestamp overflow")},
        {"MinIntSubOneViaNanos",
         [] {
           return CheckedSub(
               absl::FromUnixSeconds(std::numeric_limits<int64_t>::min()),
               absl::Nanoseconds(1));
         },
         absl::OutOfRangeError("integer overflow")},
        {"MinTimestampSubOneViaNanosScaleOverflow",
         [] {
           return CheckedSub(
               absl::FromUnixSeconds(-62135596800) + absl::Nanoseconds(1),
               absl::Nanoseconds(999999999));
         },
         absl::OutOfRangeError("timestamp overflow")},
        {"SecondsSubInfinity",
         [] {
           return CheckedSub(
               absl::FromUnixSeconds(1) + absl::Nanoseconds(999999999),
               absl::InfiniteDuration());
         },
         absl::OutOfRangeError("integer overflow")},
        {"SecondsSubNegInfinity",
         [] {
           return CheckedSub(
               absl::FromUnixSeconds(1) + absl::Nanoseconds(999999999),
               -absl::InfiniteDuration());
         },
         absl::OutOfRangeError("integer overflow")},
    }),
    [](const testing::TestParamInfo<CheckedTimeResultTest::ParamType>& info) {
      return info.param.test_name;
    });

using ConvertInt64Int32TestCase = TestCase<int32_t>;
using CheckedConvertInt64Int32Test =
    testing::TestWithParam<ConvertInt64Int32TestCase>;
TEST_P(CheckedConvertInt64Int32Test, Conversions) { ExpectResult(GetParam()); }

INSTANTIATE_TEST_SUITE_P(
    CheckedConvertInt64Int32Test, CheckedConvertInt64Int32Test,
    ValuesIn(std::vector<ConvertInt64Int32TestCase>{
        {"SimpleConversion", [] { return CheckedInt64ToInt32(1L); }, 1},
        {"Int32MaxConversion",
         [] {
           return CheckedInt64ToInt32(
               static_cast<int64_t>(std::numeric_limits<int32_t>::max()));
         },
         std::numeric_limits<int32_t>::max()},
        {"Int32MaxConversionError",
         [] {
           return CheckedInt64ToInt32(
               static_cast<int64_t>(std::numeric_limits<int64_t>::max()));
         },
         absl::OutOfRangeError("out of int32 range")},
        {"Int32MinConversion",
         [] {
           return CheckedInt64ToInt32(
               static_cast<int64_t>(std::numeric_limits<int32_t>::lowest()));
         },
         std::numeric_limits<int32_t>::lowest()},
        {"Int32MinConversionError",
         [] {
           return CheckedInt64ToInt32(
               static_cast<int64_t>(std::numeric_limits<int64_t>::lowest()));
         },
         absl::OutOfRangeError("out of int32 range")},
    }),
    [](const testing::TestParamInfo<CheckedConvertInt64Int32Test::ParamType>&
           info) { return info.param.test_name; });

using ConvertUint64Uint32TestCase = TestCase<uint32_t>;
using CheckedConvertUint64Uint32Test =
    testing::TestWithParam<ConvertUint64Uint32TestCase>;
TEST_P(CheckedConvertUint64Uint32Test, Conversions) {
  ExpectResult(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    CheckedConvertUint64Uint32Test, CheckedConvertUint64Uint32Test,
    ValuesIn(std::vector<ConvertUint64Uint32TestCase>{
        {"SimpleConversion", [] { return CheckedUint64ToUint32(1UL); }, 1U},
        {"Uint32MaxConversion",
         [] {
           return CheckedUint64ToUint32(
               static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()));
         },
         std::numeric_limits<uint32_t>::max()},
        {"Uint32MaxConversionError",
         [] {
           return CheckedUint64ToUint32(
               static_cast<uint64_t>(std::numeric_limits<uint64_t>::max()));
         },
         absl::OutOfRangeError("out of uint32 range")},
    }),
    [](const testing::TestParamInfo<CheckedConvertUint64Uint32Test::ParamType>&
           info) { return info.param.test_name; });

}  // namespace
}  // namespace cel::internal
