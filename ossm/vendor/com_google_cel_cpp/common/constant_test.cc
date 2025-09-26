// Copyright 2024 Google LLC
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

#include "common/constant.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;

TEST(Constant, NullValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_null_value(), IsFalse());
  const_expr.set_null_value();
  EXPECT_THAT(const_expr.has_null_value(), IsTrue());
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<std::nullptr_t>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kNull);
}

TEST(Constant, BoolValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_bool_value(), IsFalse());
  EXPECT_EQ(const_expr.bool_value(), false);
  const_expr.set_bool_value(false);
  EXPECT_THAT(const_expr.has_bool_value(), IsTrue());
  EXPECT_EQ(const_expr.bool_value(), false);
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<bool>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kBool);
}

TEST(Constant, IntValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_int_value(), IsFalse());
  EXPECT_EQ(const_expr.int_value(), 0);
  const_expr.set_int_value(0);
  EXPECT_THAT(const_expr.has_int_value(), IsTrue());
  EXPECT_EQ(const_expr.int_value(), 0);
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<int64_t>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kInt);
}

TEST(Constant, UintValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_uint_value(), IsFalse());
  EXPECT_EQ(const_expr.uint_value(), 0);
  const_expr.set_uint_value(0);
  EXPECT_THAT(const_expr.has_uint_value(), IsTrue());
  EXPECT_EQ(const_expr.uint_value(), 0);
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<uint64_t>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kUint);
}

TEST(Constant, DoubleValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_double_value(), IsFalse());
  EXPECT_EQ(const_expr.double_value(), 0);
  const_expr.set_double_value(0);
  EXPECT_THAT(const_expr.has_double_value(), IsTrue());
  EXPECT_EQ(const_expr.double_value(), 0);
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<double>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kDouble);
}

TEST(Constant, BytesValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_bytes_value(), IsFalse());
  EXPECT_THAT(const_expr.bytes_value(), IsEmpty());
  const_expr.set_bytes_value("foo");
  EXPECT_THAT(const_expr.has_bytes_value(), IsTrue());
  EXPECT_EQ(const_expr.bytes_value(), "foo");
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<BytesConstant>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kBytes);
}

TEST(Constant, StringValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_string_value(), IsFalse());
  EXPECT_THAT(const_expr.string_value(), IsEmpty());
  const_expr.set_string_value("foo");
  EXPECT_THAT(const_expr.has_string_value(), IsTrue());
  EXPECT_EQ(const_expr.string_value(), "foo");
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<StringConstant>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kString);
}

TEST(Constant, DurationValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_duration_value(), IsFalse());
  EXPECT_EQ(const_expr.duration_value(), absl::ZeroDuration());
  const_expr.set_duration_value(absl::ZeroDuration());
  EXPECT_THAT(const_expr.has_duration_value(), IsTrue());
  EXPECT_EQ(const_expr.duration_value(), absl::ZeroDuration());
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<absl::Duration>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kDuration);
}

TEST(Constant, TimestampValue) {
  Constant const_expr;
  EXPECT_THAT(const_expr.has_timestamp_value(), IsFalse());
  EXPECT_EQ(const_expr.timestamp_value(), absl::UnixEpoch());
  const_expr.set_timestamp_value(absl::UnixEpoch());
  EXPECT_THAT(const_expr.has_timestamp_value(), IsTrue());
  EXPECT_EQ(const_expr.timestamp_value(), absl::UnixEpoch());
  EXPECT_EQ(const_expr.kind().index(), ConstantKindIndexOf<absl::Time>());
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kTimestamp);
}

TEST(Constant, DefaultConstructed) {
  Constant const_expr;
  EXPECT_EQ(const_expr.kind_case(), ConstantKindCase::kUnspecified);
}

TEST(Constant, Equality) {
  EXPECT_EQ(Constant{}, Constant{});

  Constant lhs_const_expr;
  Constant rhs_const_expr;

  lhs_const_expr.set_null_value();
  rhs_const_expr.set_null_value();
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_bool_value(false);
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_bool_value(false);
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_int_value(0);
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_int_value(0);
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_uint_value(0);
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_uint_value(0);
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_double_value(0);
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_double_value(0);
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_bytes_value("foo");
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_bytes_value("foo");
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_string_value("foo");
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_string_value("foo");
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_duration_value(absl::ZeroDuration());
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_duration_value(absl::ZeroDuration());
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);

  lhs_const_expr.set_timestamp_value(absl::UnixEpoch());
  rhs_const_expr.set_null_value();
  EXPECT_NE(lhs_const_expr, rhs_const_expr);
  EXPECT_NE(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
  rhs_const_expr.set_timestamp_value(absl::UnixEpoch());
  EXPECT_EQ(lhs_const_expr, rhs_const_expr);
  EXPECT_EQ(rhs_const_expr, lhs_const_expr);
  EXPECT_NE(lhs_const_expr, Constant{});
  EXPECT_NE(Constant{}, rhs_const_expr);
}

std::string Stringify(const Constant& constant) {
  return absl::StrFormat("%v", constant);
}

TEST(Constant, HasAbslStringify) {
  EXPECT_TRUE(absl::HasAbslStringify<Constant>::value);
}

TEST(Constant, AbslStringify) {
  Constant constant;
  EXPECT_EQ(Stringify(constant), "<unspecified>");
  constant.set_null_value();
  EXPECT_EQ(Stringify(constant), "null");
  constant.set_bool_value(true);
  EXPECT_EQ(Stringify(constant), "true");
  constant.set_int_value(1);
  EXPECT_EQ(Stringify(constant), "1");
  constant.set_uint_value(1);
  EXPECT_EQ(Stringify(constant), "1u");
  constant.set_double_value(1);
  EXPECT_EQ(Stringify(constant), "1.0");
  constant.set_double_value(1.1);
  EXPECT_EQ(Stringify(constant), "1.1");
  constant.set_double_value(NAN);
  EXPECT_EQ(Stringify(constant), "nan");
  constant.set_double_value(INFINITY);
  EXPECT_EQ(Stringify(constant), "+infinity");
  constant.set_double_value(-INFINITY);
  EXPECT_EQ(Stringify(constant), "-infinity");
  constant.set_bytes_value("foo");
  EXPECT_EQ(Stringify(constant), "b\"foo\"");
  constant.set_string_value("foo");
  EXPECT_EQ(Stringify(constant), "\"foo\"");
  constant.set_duration_value(absl::Seconds(1));
  EXPECT_EQ(Stringify(constant), "duration(\"1s\")");
  constant.set_timestamp_value(absl::UnixEpoch() + absl::Seconds(1));
  EXPECT_EQ(Stringify(constant), "timestamp(\"1970-01-01T00:00:01Z\")");
}

}  // namespace
}  // namespace cel
