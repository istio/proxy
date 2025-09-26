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

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/time.h"

namespace google::api::expr::runtime {
namespace {

using google::protobuf::Duration;
using google::protobuf::Timestamp;

using cel::expr::Expr;
using cel::expr::SourceInfo;

using google::protobuf::Arena;

using ::cel::internal::MaxDuration;
using ::cel::internal::MinDuration;
using ::cel::internal::MinTimestamp;
using ::testing::Eq;

class BuiltinsTest : public ::testing::Test {
 protected:
  BuiltinsTest() {}

  // Helper method. Looks up in registry and tests comparison operation.
  void PerformRun(absl::string_view operation, absl::optional<CelValue> target,
                  const std::vector<CelValue>& values, CelValue* result) {
    PerformRun(operation, target, values, result, options_);
  }

  // Helper method. Looks up in registry and tests comparison operation.
  void PerformRun(absl::string_view operation, absl::optional<CelValue> target,
                  const std::vector<CelValue>& values, CelValue* result,
                  const InterpreterOptions& options) {
    Activation activation;

    Expr expr;
    SourceInfo source_info;
    auto call = expr.mutable_call_expr();
    call->set_function(operation);

    if (target.has_value()) {
      std::string param_name = "target";
      activation.InsertValue(param_name, target.value());

      auto target_arg = call->mutable_target();
      auto ident = target_arg->mutable_ident_expr();
      ident->set_name(param_name);
    }

    int counter = 0;
    for (const auto& value : values) {
      std::string param_name = absl::StrCat("param_", counter++);

      activation.InsertValue(param_name, value);
      auto arg = call->add_args();
      auto ident = arg->mutable_ident_expr();
      ident->set_name(param_name);
    }

    // Obtain CEL Expression builder.
    std::unique_ptr<CelExpressionBuilder> builder =
        CreateCelExpressionBuilder(options);

    // Builtin registration.
    ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

    // Create CelExpression from AST (Expr object).
    ASSERT_OK_AND_ASSIGN(auto cel_expression,
                         builder->CreateExpression(&expr, &source_info));
    ASSERT_OK_AND_ASSIGN(auto value,
                         cel_expression->Evaluate(activation, &arena_));
    *result = value;
  }

  // Helper method. Looks up in registry and tests comparison operation.
  void TestComparison(absl::string_view operation, const CelValue& ref,
                      const CelValue& other, bool result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(
        PerformRun(operation, {}, {ref, other}, &result_value));

    ASSERT_EQ(result_value.IsBool(), true)
        << absl::StrCat(CelValue::TypeName(ref.type()), " ", operation, " ",
                        CelValue::TypeName(other.type()));
    ASSERT_EQ(result_value.BoolOrDie(), result)
        << operation << " for " << ref.DebugString() << " with "
        << other.DebugString();
  }

  // Helper method. Looks up in registry and tests for no matching equality
  // overload.
  void TestNoMatchingEqualOverload(const CelValue& ref, const CelValue& other) {
    options_.enable_heterogeneous_equality = false;
    CelValue eq_value;
    ASSERT_NO_FATAL_FAILURE(
        PerformRun(builtin::kEqual, {}, {ref, other}, &eq_value, options_));
    ASSERT_TRUE(eq_value.IsError())
        << " for " << CelValue::TypeName(ref.type()) << " and "
        << CelValue::TypeName(other.type());
    EXPECT_TRUE(CheckNoMatchingOverloadError(eq_value));

    CelValue ineq_value;
    ASSERT_NO_FATAL_FAILURE(
        PerformRun(builtin::kInequal, {}, {ref, other}, &ineq_value, options_));
    ASSERT_TRUE(ineq_value.IsError())
        << " for " << CelValue::TypeName(ref.type()) << " and "
        << CelValue::TypeName(other.type());
    EXPECT_TRUE(CheckNoMatchingOverloadError(ineq_value));
  }

  // Helper method. Looks up in registry and tests Type conversions.
  void TestTypeConverts(absl::string_view operation, const CelValue& ref,
                        CelValue::BytesHolder result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {}, {ref}, &result_value));

    ASSERT_EQ(result_value.IsBytes(), true);
    ASSERT_EQ(result_value.BytesOrDie(), result)
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  // Helper method. Looks up in registry and tests Type conversions.
  void TestTypeConverts(absl::string_view operation, const CelValue& ref,
                        CelValue::StringHolder result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {}, {ref}, &result_value));

    ASSERT_EQ(result_value.IsString(), true);
    ASSERT_EQ(result_value.StringOrDie().value(), result.value())
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  void TestTypeConverts(absl::string_view operation, const CelValue& ref,
                        double result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {}, {ref}, &result_value));

    ASSERT_EQ(result_value.IsDouble(), true);
    ASSERT_EQ(result_value.DoubleOrDie(), result)
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  void TestTypeConverts(absl::string_view operation, const CelValue& ref,
                        int64_t result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {}, {ref}, &result_value));

    ASSERT_EQ(result_value.IsInt64(), true);
    ASSERT_EQ(result_value.Int64OrDie(), result)
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  void TestTypeConverts(absl::string_view operation, const CelValue& ref,
                        uint64_t result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {}, {ref}, &result_value));

    ASSERT_EQ(result_value.IsUint64(), true);
    ASSERT_EQ(result_value.Uint64OrDie(), result)
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  void TestTypeConverts(absl::string_view operation, const CelValue& ref,
                        Duration& result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {}, {ref}, &result_value));

    ASSERT_EQ(result_value.IsDuration(), true);
    ASSERT_EQ(result_value.DurationOrDie(),
              CelProtoWrapper::CreateDuration(&result).DurationOrDie())
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  // Helper method. Attempts to perform a type conversion and expects an error
  // as the result.
  void TestTypeConversionError(absl::string_view operation,
                               const CelValue& ref) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {}, {ref}, &result_value));

    ASSERT_EQ(result_value.IsError(), true) << result_value.DebugString();
  }

  // Helper method. Looks up in registry and tests functions without params.
  void TestFunctions(absl::string_view operation, const CelValue& ref,
                     int64_t result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(operation, {ref}, {}, &result_value));

    ASSERT_EQ(result_value.IsInt64(), true);
    ASSERT_EQ(result_value.Int64OrDie(), result)
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  // Helper method. Looks up in registry and tests functions with params.
  void TestFunctionsWithParams(absl::string_view operation, const CelValue& ref,
                               const std::vector<CelValue>& params,
                               int64_t result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(
        PerformRun(operation, {ref}, {params}, &result_value));

    ASSERT_EQ(result_value.IsInt64(), true);
    ASSERT_EQ(result_value.Int64OrDie(), result)
        << operation << " for " << CelValue::TypeName(ref.type());
  }

  // Helper method to test && and || operations
  void TestLogicalOperation(absl::string_view operation, bool v1, bool v2,
                            bool result) {
    CelValue result_value;

    ASSERT_NO_FATAL_FAILURE(PerformRun(
        operation, {}, {CelValue::CreateBool(v1), CelValue::CreateBool(v2)},
        &result_value));

    ASSERT_EQ(result_value.IsBool(), true);

    ASSERT_EQ(result_value.BoolOrDie(), result) << operation;
  }

  void TestComparisonsForType(CelValue::Type kind, const CelValue& ref,
                              const CelValue& lesser) {
    std::string type_name = CelValue::TypeName(kind);

    TestComparison(builtin::kEqual, ref, ref, true);
    TestComparison(builtin::kEqual, ref, lesser, false);

    TestComparison(builtin::kInequal, ref, ref, false);
    TestComparison(builtin::kInequal, ref, lesser, true);

    TestComparison(builtin::kLess, ref, ref, false);
    TestComparison(builtin::kLess, ref, lesser, false);
    TestComparison(builtin::kLess, lesser, ref, true);

    TestComparison(builtin::kLessOrEqual, ref, ref, true);
    TestComparison(builtin::kLessOrEqual, ref, lesser, false);
    TestComparison(builtin::kLessOrEqual, lesser, ref, true);

    TestComparison(builtin::kGreater, ref, ref, false);
    TestComparison(builtin::kGreater, ref, lesser, true);
    TestComparison(builtin::kGreater, lesser, ref, false);

    TestComparison(builtin::kGreaterOrEqual, ref, ref, true);
    TestComparison(builtin::kGreaterOrEqual, ref, lesser, true);
    TestComparison(builtin::kGreaterOrEqual, lesser, ref, false);
  }

  // Helper method to test arithmetical operations for Int64
  void TestArithmeticalOperationInt64(absl::string_view operation, int64_t v1,
                                      int64_t v2, int64_t result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        operation, {}, {CelValue::CreateInt64(v1), CelValue::CreateInt64(v2)},
        &result_value));

    ASSERT_EQ(result_value.IsInt64(), true);
    ASSERT_EQ(result_value.Int64OrDie(), result) << operation;
  }

  // Helper for testing invalid signed integer arithmetic operations.
  void TestArithmeticalErrorInt64(absl::string_view operation, int64_t v1,
                                  int64_t v2, absl::StatusCode code) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        operation, {}, {CelValue::CreateInt64(v1), CelValue::CreateInt64(v2)},
        &result_value));
    ASSERT_EQ(result_value.IsError(), true);
    ASSERT_EQ(result_value.ErrorOrDie()->code(), code) << operation;
  }

  // Helper method to test arithmetical operations for Uint64
  void TestArithmeticalOperationUint64(absl::string_view operation, uint64_t v1,
                                       uint64_t v2, uint64_t result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        operation, {}, {CelValue::CreateUint64(v1), CelValue::CreateUint64(v2)},
        &result_value));
    ASSERT_EQ(result_value.IsUint64(), true);
    ASSERT_EQ(result_value.Uint64OrDie(), result) << operation;
  }

  // Helper for testing invalid unsigned integer arithmetic operations.
  void TestArithmeticalErrorUint64(absl::string_view operation, uint64_t v1,
                                   uint64_t v2, absl::StatusCode code) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        operation, {}, {CelValue::CreateUint64(v1), CelValue::CreateUint64(v2)},
        &result_value));
    ASSERT_EQ(result_value.IsError(), true);
    ASSERT_EQ(result_value.ErrorOrDie()->code(), code) << operation;
  }

  // Helper method to test arithmetical operations for Double
  void TestArithmeticalOperationDouble(absl::string_view operation, double v1,
                                       double v2, double result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        operation, {}, {CelValue::CreateDouble(v1), CelValue::CreateDouble(v2)},
        &result_value));

    ASSERT_EQ(result_value.IsDouble(), true);
    ASSERT_DOUBLE_EQ(result_value.DoubleOrDie(), result) << operation;
  }

  void TestInList(const CelList* cel_list, const CelValue& value, bool result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kIn, {},
                                       {value, CelValue::CreateList(cel_list)},
                                       &result_value));

    ASSERT_EQ(result_value.IsBool(), true)
        << result_value.DebugString() << " argument: " << value.DebugString();
    ASSERT_EQ(result_value.BoolOrDie(), result)
        << " for " << CelValue::TypeName(value.type());
  }

  void TestInDeprecatedList(const CelList* cel_list, const CelValue& value,
                            bool result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kInDeprecated, {},
                                       {value, CelValue::CreateList(cel_list)},
                                       &result_value));

    ASSERT_EQ(result_value.IsBool(), true);
    ASSERT_EQ(result_value.BoolOrDie(), result)
        << " for " << CelValue::TypeName(value.type());
  }

  void TestInFunctionList(const CelList* cel_list, const CelValue& value,
                          bool result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kInFunction, {},
                                       {value, CelValue::CreateList(cel_list)},
                                       &result_value));

    ASSERT_EQ(result_value.IsBool(), true);
    ASSERT_EQ(result_value.BoolOrDie(), result)
        << " for " << CelValue::TypeName(value.type());
  }

  void TestInMap(const CelMap* cel_map, const CelValue& value, bool result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kIn, {},
                                       {value, CelValue::CreateMap(cel_map)},
                                       &result_value, options_));

    ASSERT_EQ(result_value.IsBool(), true);
    ASSERT_EQ(result_value.BoolOrDie(), result)
        << " for " << value.DebugString();
  }

  void TestInDeprecatedMap(const CelMap* cel_map, const CelValue& value,
                           bool result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kInDeprecated, {},
                                       {value, CelValue::CreateMap(cel_map)},
                                       &result_value, options_));

    ASSERT_EQ(result_value.IsBool(), true);
    ASSERT_EQ(result_value.BoolOrDie(), result)
        << " for " << CelValue::TypeName(value.type());
  }

  void TestInFunctionMap(const CelMap* cel_map, const CelValue& value,
                         bool result) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kInFunction, {},
                                       {value, CelValue::CreateMap(cel_map)},
                                       &result_value, options_));

    ASSERT_EQ(result_value.IsBool(), true);
    ASSERT_EQ(result_value.BoolOrDie(), result)
        << " for " << CelValue::TypeName(value.type());
  }

  InterpreterOptions options_;

  // Arena
  Arena arena_;
};

class HeterogeneousEqualityTest : public BuiltinsTest {
 public:
  HeterogeneousEqualityTest() { options_.enable_heterogeneous_equality = true; }
};

// Test Not() operation for Bool
TEST_F(BuiltinsTest, TestNotOp) {
  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kNot, {}, {CelValue::CreateBool(true)}, &result));

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

// Test negation operation for numeric types.
TEST_F(BuiltinsTest, TestNegOp) {
  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kNeg, {}, {CelValue::CreateInt64(-1)}, &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);

  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kNeg, {}, {CelValue::CreateDouble(-1.0)}, &result));
  ASSERT_TRUE(result.IsDouble());
  EXPECT_EQ(result.DoubleOrDie(), 1.0);
}

// Test integer negation overflow.
TEST_F(BuiltinsTest, TestNegIntOverflow) {
  CelValue result;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kNeg, {},
      {CelValue::CreateInt64(std::numeric_limits<int64_t>::lowest())},
      &result));
  ASSERT_TRUE(result.IsError());
}

// Test Equality/Non-Equality operation for Bool
TEST_F(BuiltinsTest, TestBoolEqual) {
  CelValue ref = CelValue::CreateBool(true);
  CelValue lesser = CelValue::CreateBool(false);

  TestComparisonsForType(CelValue::Type::kBool, ref, lesser);
}

// Test Equality/Non-Equality operation for Int64
TEST_F(BuiltinsTest, TestInt64Equal) {
  CelValue ref = CelValue::CreateInt64(2);
  CelValue lesser = CelValue::CreateInt64(1);

  TestComparisonsForType(CelValue::Type::kInt64, ref, lesser);
}

// Test Equality/Non-Equality operation for Uint64
TEST_F(BuiltinsTest, TestUint64Comparisons) {
  CelValue ref = CelValue::CreateUint64(2);
  CelValue lesser = CelValue::CreateUint64(1);

  TestComparisonsForType(CelValue::Type::kUint64, ref, lesser);
}

// Test Equality/Non-Equality operation for Double
TEST_F(BuiltinsTest, TestDoubleComparisons) {
  CelValue ref = CelValue::CreateDouble(2);
  CelValue lesser = CelValue::CreateDouble(1);

  TestComparisonsForType(CelValue::Type::kDouble, ref, lesser);
}

// Test Equality/Non-Equality operation for String
TEST_F(BuiltinsTest, TestStringEqual) {
  std::string test1 = "test1";
  std::string test2 = "test2";

  CelValue ref = CelValue::CreateString(&test2);
  CelValue lesser = CelValue::CreateString(&test1);

  TestComparisonsForType(CelValue::Type::kString, ref, lesser);
}

// Test Equality/Non-Equality operation for Double
TEST_F(BuiltinsTest, TestDurationComparisons) {
  Duration ref;
  Duration lesser;

  ref.set_seconds(2);
  ref.set_nanos(1);

  lesser.set_seconds(1);
  lesser.set_nanos(2);

  TestComparisonsForType(CelValue::Type::kDuration,
                         CelProtoWrapper::CreateDuration(&ref),
                         CelProtoWrapper::CreateDuration(&lesser));
}

// Test Equality/Non-Equality operation for messages
TEST_F(BuiltinsTest, TestNullMessageEqual) {
  CelValue ref = CelValue::CreateNull();
  Expr dummy;
  CelValue value = CelProtoWrapper::CreateMessage(&dummy, &arena_);
  TestComparison(builtin::kEqual, ref, ref, true);
  TestComparison(builtin::kInequal, ref, ref, false);
  TestComparison(builtin::kEqual, value, ref, false);
  TestComparison(builtin::kInequal, value, ref, true);
  TestComparison(builtin::kEqual, ref, value, false);
  TestComparison(builtin::kInequal, ref, value, true);
}

// Test functions for Duration
TEST_F(BuiltinsTest, TestDurationFunctions) {
  Duration ref;

  ref.set_seconds(93541L);
  ref.set_nanos(11000000L);

  TestFunctions(builtin::kHours, CelProtoWrapper::CreateDuration(&ref), 25L);
  TestFunctions(builtin::kMinutes, CelProtoWrapper::CreateDuration(&ref),
                1559L);
  TestFunctions(builtin::kSeconds, CelProtoWrapper::CreateDuration(&ref),
                93541L);
  TestFunctions(builtin::kMilliseconds, CelProtoWrapper::CreateDuration(&ref),
                11L);

  std::string result = "93541.011s";
  TestTypeConverts(builtin::kString, CelProtoWrapper::CreateDuration(&ref),
                   CelValue::StringHolder(&result));
  TestTypeConverts(builtin::kDuration, CelValue::CreateString(&result), ref);

  ref.set_seconds(-93541L);
  ref.set_nanos(-11000000L);

  TestFunctions(builtin::kHours, CelProtoWrapper::CreateDuration(&ref), -25L);
  TestFunctions(builtin::kMinutes, CelProtoWrapper::CreateDuration(&ref),
                -1559L);
  TestFunctions(builtin::kSeconds, CelProtoWrapper::CreateDuration(&ref),
                -93541L);
  TestFunctions(builtin::kMilliseconds, CelProtoWrapper::CreateDuration(&ref),
                -11L);

  result = "-93541.011s";
  TestTypeConverts(builtin::kString, CelProtoWrapper::CreateDuration(&ref),
                   CelValue::StringHolder(&result));
  TestTypeConverts(builtin::kDuration, CelValue::CreateString(&result), ref);

  absl::Duration d = MinDuration() + absl::Seconds(-1);
  result = absl::FormatDuration(d);
  TestTypeConversionError(builtin::kDuration, CelValue::CreateString(&result));

  d = MaxDuration() + absl::Seconds(1);
  result = absl::FormatDuration(d);
  TestTypeConversionError(builtin::kDuration, CelValue::CreateString(&result));

  std::string inf = "inf";
  std::string ninf = "-inf";
  TestTypeConversionError(builtin::kDuration, CelValue::CreateString(&inf));
  TestTypeConversionError(builtin::kDuration, CelValue::CreateString(&ninf));
}

// Test functions for Timestamp
TEST_F(BuiltinsTest, TestTimestampFunctions) {
  Timestamp ref;

  // Test timestamp functions w/o timezone
  ref.set_seconds(1L);
  ref.set_nanos(11000000L);
  TestFunctions(builtin::kFullYear, CelProtoWrapper::CreateTimestamp(&ref),
                1970L);
  TestFunctions(builtin::kMonth, CelProtoWrapper::CreateTimestamp(&ref), 0L);
  TestFunctions(builtin::kDayOfYear, CelProtoWrapper::CreateTimestamp(&ref),
                0L);
  TestFunctions(builtin::kDayOfMonth, CelProtoWrapper::CreateTimestamp(&ref),
                0L);
  TestFunctions(builtin::kDate, CelProtoWrapper::CreateTimestamp(&ref), 1L);
  TestFunctions(builtin::kHours, CelProtoWrapper::CreateTimestamp(&ref), 0L);
  TestFunctions(builtin::kMinutes, CelProtoWrapper::CreateTimestamp(&ref), 0L);
  TestFunctions(builtin::kSeconds, CelProtoWrapper::CreateTimestamp(&ref), 1L);
  TestFunctions(builtin::kMilliseconds, CelProtoWrapper::CreateTimestamp(&ref),
                11L);

  ref.set_seconds(259200L);
  ref.set_nanos(0L);
  TestFunctions(builtin::kDayOfWeek, CelProtoWrapper::CreateTimestamp(&ref),
                0L);
}

TEST_F(BuiltinsTest, TestTimestampConversionToString) {
  Timestamp ref;
  ref.set_seconds(1L);
  ref.set_nanos(11000000L);
  std::string result = "1970-01-01T00:00:01.011Z";
  TestTypeConverts(builtin::kString, CelProtoWrapper::CreateTimestamp(&ref),
                   CelValue::StringHolder(&result));

  ref.set_seconds(259200L);
  ref.set_nanos(0L);
  result = "1970-01-04T00:00:00Z";
  TestTypeConverts(builtin::kString, CelProtoWrapper::CreateTimestamp(&ref),
                   CelValue::StringHolder(&result));
}

TEST_F(BuiltinsTest, TestTimestampFunctionsWithTimeZone) {
  // Test timestamp functions w/ IANA timezone
  Timestamp ref;
  ref.set_seconds(1L);
  ref.set_nanos(11000000L);
  std::vector<CelValue> params;
  const std::string timezone = "America/Los_Angeles";
  params.push_back(CelValue::CreateString(&timezone));

  TestFunctionsWithParams(builtin::kFullYear,
                          CelProtoWrapper::CreateTimestamp(&ref), params,
                          1969L);
  TestFunctionsWithParams(builtin::kMonth,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 11L);
  TestFunctionsWithParams(builtin::kDayOfYear,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 364L);
  TestFunctionsWithParams(builtin::kDayOfMonth,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 30L);
  TestFunctionsWithParams(builtin::kDate,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 31L);
  TestFunctionsWithParams(builtin::kHours,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 16L);
  TestFunctionsWithParams(builtin::kMinutes,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 0L);
  TestFunctionsWithParams(builtin::kSeconds,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 1L);
  TestFunctionsWithParams(builtin::kMilliseconds,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 11L);

  ref.set_seconds(259200L);
  ref.set_nanos(0L);
  TestFunctionsWithParams(builtin::kDayOfWeek,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 6L);

  // Test timestamp functions with negative value
  ref.set_seconds(-1L);
  ref.set_nanos(0L);

  TestFunctions(builtin::kFullYear, CelProtoWrapper::CreateTimestamp(&ref),
                1969L);
  TestFunctions(builtin::kMonth, CelProtoWrapper::CreateTimestamp(&ref), 11L);
  TestFunctions(builtin::kDayOfYear, CelProtoWrapper::CreateTimestamp(&ref),
                364L);
  TestFunctions(builtin::kDayOfMonth, CelProtoWrapper::CreateTimestamp(&ref),
                30L);
  TestFunctions(builtin::kDate, CelProtoWrapper::CreateTimestamp(&ref), 31L);
  TestFunctions(builtin::kHours, CelProtoWrapper::CreateTimestamp(&ref), 23L);
  TestFunctions(builtin::kMinutes, CelProtoWrapper::CreateTimestamp(&ref), 59L);
  TestFunctions(builtin::kSeconds, CelProtoWrapper::CreateTimestamp(&ref), 59L);
  TestFunctions(builtin::kDayOfWeek, CelProtoWrapper::CreateTimestamp(&ref),
                3L);

  // Test timestamp functions w/ fixed timezone
  ref.set_seconds(1L);
  ref.set_nanos(11000000L);
  const std::string fixedzone = "-08:00";
  params.clear();
  params.push_back(CelValue::CreateString(&fixedzone));

  TestFunctionsWithParams(builtin::kFullYear,
                          CelProtoWrapper::CreateTimestamp(&ref), params,
                          1969L);
  TestFunctionsWithParams(builtin::kMonth,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 11L);
  TestFunctionsWithParams(builtin::kDayOfYear,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 364L);
  TestFunctionsWithParams(builtin::kDayOfMonth,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 30L);
  TestFunctionsWithParams(builtin::kDate,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 31L);
  TestFunctionsWithParams(builtin::kHours,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 16L);
  TestFunctionsWithParams(builtin::kMinutes,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 0L);
  TestFunctionsWithParams(builtin::kSeconds,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 1L);
  TestFunctionsWithParams(builtin::kMilliseconds,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 11L);

  ref.set_seconds(259200L);
  ref.set_nanos(0L);
  TestFunctionsWithParams(builtin::kDayOfWeek,
                          CelProtoWrapper::CreateTimestamp(&ref), params, 6L);

  // Test timestamp functions with negative value
  ref.set_seconds(-1L);
  ref.set_nanos(0L);

  TestFunctions(builtin::kFullYear, CelProtoWrapper::CreateTimestamp(&ref),
                1969L);
  TestFunctions(builtin::kMonth, CelProtoWrapper::CreateTimestamp(&ref), 11L);
  TestFunctions(builtin::kDayOfYear, CelProtoWrapper::CreateTimestamp(&ref),
                364L);
  TestFunctions(builtin::kDayOfMonth, CelProtoWrapper::CreateTimestamp(&ref),
                30L);
  TestFunctions(builtin::kDate, CelProtoWrapper::CreateTimestamp(&ref), 31L);
  TestFunctions(builtin::kHours, CelProtoWrapper::CreateTimestamp(&ref), 23L);
  TestFunctions(builtin::kMinutes, CelProtoWrapper::CreateTimestamp(&ref), 59L);
  TestFunctions(builtin::kSeconds, CelProtoWrapper::CreateTimestamp(&ref), 59L);
  TestFunctions(builtin::kDayOfWeek, CelProtoWrapper::CreateTimestamp(&ref),
                3L);

  TestTypeConversionError(
      builtin::kString,
      CelValue::CreateTimestamp(MinTimestamp() + absl::Seconds(-1)));
}

TEST_F(BuiltinsTest, TestBytesConversions_bytes) {
  std::string txt = "hello";
  CelValue::BytesHolder result = CelValue::BytesHolder(&txt);
  TestTypeConverts(builtin::kBytes, CelValue::CreateBytes(&txt), result);
}

TEST_F(BuiltinsTest, TestBytesConversions_string) {
  std::string txt = "hello";
  CelValue::BytesHolder result = CelValue::BytesHolder(&txt);
  TestTypeConverts(builtin::kBytes, CelValue::CreateString(&txt), result);
}

TEST_F(BuiltinsTest, TestDoubleConversions_double) {
  double ref = 100.1;
  TestTypeConverts(builtin::kDouble, CelValue::CreateDouble(ref), 100.1);
}

TEST_F(BuiltinsTest, TestDoubleConversions_int) {
  int64_t ref = 100L;
  TestTypeConverts(builtin::kDouble, CelValue::CreateInt64(ref), 100.0);
}

TEST_F(BuiltinsTest, TestDoubleConversions_string) {
  std::string ref = "-100.1";
  TestTypeConverts(builtin::kDouble, CelValue::CreateString(&ref), -100.1);
}

TEST_F(BuiltinsTest, TestDoubleConversions_uint) {
  uint64_t ref = 100UL;
  TestTypeConverts(builtin::kDouble, CelValue::CreateUint64(ref), 100.0);
}

TEST_F(BuiltinsTest, TestDoubleConversionError_stringInvalid) {
  std::string invalid = "-100e-10.0";
  TestTypeConversionError(builtin::kDouble, CelValue::CreateString(&invalid));
}

TEST_F(BuiltinsTest, TestDynConversions) {
  TestTypeConverts(builtin::kDyn, CelValue::CreateDouble(100.1), 100.1);
  TestTypeConverts(builtin::kDyn, CelValue::CreateInt64(100L), 100L);
  TestTypeConverts(builtin::kDyn, CelValue::CreateUint64(100UL), 100UL);
}

TEST_F(BuiltinsTest, TestIntConversions_int) {
  TestTypeConverts(builtin::kInt, CelValue::CreateInt64(100L), 100L);
}

TEST_F(BuiltinsTest, TestIntConversions_Timestamp) {
  Timestamp ref;
  ref.set_seconds(100);
  TestTypeConverts(builtin::kInt, CelProtoWrapper::CreateTimestamp(&ref), 100L);
}

TEST_F(BuiltinsTest, TestIntConversions_double) {
  double ref = 100.1;
  TestTypeConverts(builtin::kInt, CelValue::CreateDouble(ref), 100L);
}

TEST_F(BuiltinsTest, TestIntConversions_string) {
  std::string ref = "100";
  TestTypeConverts(builtin::kInt, CelValue::CreateString(&ref), 100L);
}

TEST_F(BuiltinsTest, TestIntConversions_uint) {
  uint64_t ref = 100;
  TestTypeConverts(builtin::kInt, CelValue::CreateUint64(ref), 100L);
}

TEST_F(BuiltinsTest, TestIntConversions_doubleIntMin) {
  // Converting int64_t min may or may not roundtrip properly without overflow
  // depending on compiler flags, so the conservative approach is to treat this
  // case as overflow.
  double range = std::numeric_limits<int64_t>::lowest();
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestIntConversions_doubleIntMinMinus1024) {
  // Converting values between [int64_t::lowest(), (int64_t::lowest() - 1024)]
  // will result in an int64_t representable value, in some cases, but not all
  // as the conversion depends on
  double range = std::numeric_limits<int64_t>::lowest();
  range -= 1024L;
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestIntConversionError_doubleIntMaxMinus512) {
  // Converting int64_t max - 512 to a double will not roundtrip to the original
  // value, but it will rountrip to a valid 64-bit integer.
  double range = std::numeric_limits<int64_t>::max() - 512;
  TestTypeConverts(builtin::kInt, CelValue::CreateDouble(range),
                   std::numeric_limits<int64_t>::max() - 1023);
}

TEST_F(BuiltinsTest, TestIntConversionError_doubleNegRange) {
  double range = -1.0e99;
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestIntConversionError_doublePosRange) {
  double range = 1.0e99;
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestIntConversionError_doubleIntMax) {
  // Converting int64_t max to a double results in a double value of int64_t max
  // + 1 which should cause the overflow testing to trip.
  double range = std::numeric_limits<int64_t>::max();
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}
TEST_F(BuiltinsTest, TestIntConversionError_doubleIntMaxMinus1) {
  // Converting values between int64_t::max() and int64_t::max() - 511 will
  // result in overflow errors during round-tripping.
  double range = std::numeric_limits<int64_t>::max() - 1;
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestIntConversionError_doubleIntMaxMinus511) {
  // Converting values between int64_t::max() and int64_t::max() - 511 will
  // result in overflow errors during round-tripping.
  double range = std::numeric_limits<int64_t>::max() - 511;
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestIntConversionError_doubleIntMinMinus1025) {
  // Converting double values lower than int64_t::lowest() - 1025 will result in
  // an overflow error.
  double range = std::numeric_limits<int64_t>::lowest();
  range -= 1025L;
  TestTypeConversionError(builtin::kInt, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestIntConversionError_uintRange) {
  uint64_t range = 18446744073709551615UL;
  TestTypeConversionError(builtin::kInt, CelValue::CreateUint64(range));
}

TEST_F(BuiltinsTest, TestUintConversions_double) {
  double ref = 100.1;
  TestTypeConverts(builtin::kUint, CelValue::CreateDouble(ref), 100UL);
}

TEST_F(BuiltinsTest, TestUintConversions_int) {
  int64_t ref = 100L;
  TestTypeConverts(builtin::kUint, CelValue::CreateInt64(ref), 100UL);
}

TEST_F(BuiltinsTest, TestUintConversions_string) {
  std::string ref = "100";
  TestTypeConverts(builtin::kUint, CelValue::CreateString(&ref), 100UL);
}

TEST_F(BuiltinsTest, TestUintConversions_uint) {
  TestTypeConverts(builtin::kUint, CelValue::CreateUint64(100UL), 100UL);
}

TEST_F(BuiltinsTest, TestUintConversionError_doubleNegRange) {
  double range = -1.0e99;
  TestTypeConversionError(builtin::kUint, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestUintConversionError_doublePosRange) {
  double range = 1.0e99;
  TestTypeConversionError(builtin::kUint, CelValue::CreateDouble(range));
}

TEST_F(BuiltinsTest, TestUintConversionError_intRange) {
  int64_t range = -1L;
  TestTypeConversionError(builtin::kUint, CelValue::CreateInt64(range));
}

TEST_F(BuiltinsTest, TestUintConversionError_stringInvalid) {
  std::string invalid = "-100";
  TestTypeConversionError(builtin::kUint, CelValue::CreateString(&invalid));
}

TEST_F(BuiltinsTest, TestTimestampComparisons) {
  Timestamp ref;
  Timestamp lesser;

  ref.set_seconds(2);
  ref.set_nanos(1);

  lesser.set_seconds(1);
  lesser.set_nanos(2);

  TestComparisonsForType(CelValue::Type::kTimestamp,
                         CelProtoWrapper::CreateTimestamp(&ref),
                         CelProtoWrapper::CreateTimestamp(&lesser));
}

TEST_F(BuiltinsTest, TestLogicalOr) {
  const char* op_name = builtin::kOr;
  TestLogicalOperation(op_name, true, true, true);
  TestLogicalOperation(op_name, false, true, true);
  TestLogicalOperation(op_name, true, false, true);
  TestLogicalOperation(op_name, false, false, false);

  CelError error = absl::CancelledError();
  // Test special cases - mix of bool and error
  // true || error
  CelValue result;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateBool(true), CelValue::CreateError(&error)},
      &result));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  // error || true
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateError(&error), CelValue::CreateBool(true)},
      &result));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  // false || error
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateBool(false), CelValue::CreateError(&error)},
      &result));
  EXPECT_TRUE(result.IsError());

  // error || false
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateError(&error), CelValue::CreateBool(false)},
      &result));
  EXPECT_TRUE(result.IsError());

  // error || error
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {},
      {CelValue::CreateError(&error), CelValue::CreateError(&error)}, &result));
  EXPECT_TRUE(result.IsError());

  // "foo" || "bar"
  std::string arg0 = "foo";
  std::string arg1 = "bar";
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {},
      {CelValue::CreateString(&arg0), CelValue::CreateString(&arg1)}, &result));
  EXPECT_TRUE(CheckNoMatchingOverloadError(result));
}

TEST_F(BuiltinsTest, TestLogicalAnd) {
  const char* op_name = builtin::kAnd;
  TestLogicalOperation(op_name, true, true, true);
  TestLogicalOperation(op_name, false, true, false);
  TestLogicalOperation(op_name, true, false, false);
  TestLogicalOperation(op_name, false, false, false);

  CelError error = absl::CancelledError();
  // Test special cases - mix of bool and error
  // true && error
  CelValue result;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateBool(false), CelValue::CreateError(&error)},
      &result));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);

  // error && false
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateError(&error), CelValue::CreateBool(false)},
      &result));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);

  // false && error
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateBool(true), CelValue::CreateError(&error)},
      &result));
  EXPECT_TRUE(result.IsError());

  // error && true
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {}, {CelValue::CreateError(&error), CelValue::CreateBool(true)},
      &result));
  EXPECT_TRUE(result.IsError());

  // error && error
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      op_name, {},
      {CelValue::CreateError(&error), CelValue::CreateError(&error)}, &result));
  EXPECT_TRUE(result.IsError());
}

TEST_F(BuiltinsTest, TestTernary) {
  std::vector<CelValue> args = {CelValue::CreateBool(true),
                                CelValue::CreateInt64(1),
                                CelValue::CreateInt64(2)};
  CelValue result_value;

  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kTernary, {}, args, &result_value));

  ASSERT_EQ(result_value.IsInt64(), true);
  ASSERT_EQ(result_value.Int64OrDie(), 1);

  args[0] = CelValue::CreateBool(false);
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kTernary, {}, args, &result_value));

  ASSERT_EQ(result_value.IsInt64(), true);
  ASSERT_EQ(result_value.Int64OrDie(), 2);
}

TEST_F(BuiltinsTest, TestTernaryErrorAsCondition) {
  CelError cel_error = absl::CancelledError();
  std::vector<CelValue> args = {CelValue::CreateError(&cel_error),
                                CelValue::CreateInt64(1),
                                CelValue::CreateInt64(2)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kTernary, {}, args, &result_value));

  ASSERT_EQ(result_value.IsError(), true);
  ASSERT_EQ(*result_value.ErrorOrDie(), cel_error);
}

TEST_F(BuiltinsTest, TestTernaryStringAsCondition) {
  std::string test = "test";
  std::vector<CelValue> args = {CelValue::CreateString(&test),
                                CelValue::CreateInt64(1),
                                CelValue::CreateInt64(2)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kTernary, {}, args, &result_value));
  EXPECT_TRUE(CheckNoMatchingOverloadError(result_value));
}

class FakeList : public CelList {
 public:
  explicit FakeList(const std::vector<CelValue>& values) : values_(values) {}

  int size() const override { return values_.size(); }

  CelValue operator[](int index) const override { return values_[index]; }

 private:
  std::vector<CelValue> values_;
};

class FakeErrorMap : public CelMap {
 public:
  FakeErrorMap() {}

  int size() const override { return 0; }

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    return absl::InvalidArgumentError("bad key type");
  }

  absl::optional<CelValue> operator[](CelValue key) const override {
    return absl::nullopt;
  }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return absl::UnimplementedError("CelMap::ListKeys is not implemented");
  }
};

template <typename T>
class FakeMap : public CelMap {
 public:
  template <typename CreateCelValue, typename GetCelValue>
  FakeMap(const std::map<T, CelValue>& data,
          const CreateCelValue& create_cel_value,
          const GetCelValue& get_cel_value)
      : data_(data), get_cel_value_(get_cel_value) {
    std::vector<CelValue> keys;
    for (auto kv : data) {
      keys.push_back(create_cel_value(kv.first));
    }
    keys_ = std::make_unique<FakeList>(keys);
  }

  int size() const override { return data_.size(); }

  absl::optional<CelValue> operator[](CelValue key) const override {
    absl::optional<T> raw_value = get_cel_value_(key);
    if (!raw_value) {
      return absl::nullopt;
    }
    auto it = data_.find(*raw_value);
    if (it == data_.end()) {
      return absl::nullopt;
    }
    return it->second;
  }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return keys_.get();
  }

 private:
  std::map<T, CelValue> data_;
  std::unique_ptr<FakeList> keys_;
  std::function<absl::optional<T>(CelValue)> get_cel_value_;
};

class FakeBoolMap : public FakeMap<bool> {
 public:
  explicit FakeBoolMap(const std::map<bool, CelValue>& data)
      : FakeMap(data, CelValue::CreateBool,
                [](CelValue v) -> absl::optional<bool> {
                  if (!v.IsBool()) {
                    return absl::nullopt;
                  }
                  return v.BoolOrDie();
                }) {}
};

class FakeInt64Map : public FakeMap<int64_t> {
 public:
  explicit FakeInt64Map(const std::map<int64_t, CelValue>& data)
      : FakeMap(data, CelValue::CreateInt64,
                [](CelValue v) -> absl::optional<int64_t> {
                  if (!v.IsInt64()) {
                    return absl::nullopt;
                  }
                  return v.Int64OrDie();
                }) {}
};

class FakeUint64Map : public FakeMap<uint64_t> {
 public:
  explicit FakeUint64Map(const std::map<uint64_t, CelValue>& data)
      : FakeMap(data, CelValue::CreateUint64,
                [](CelValue v) -> absl::optional<uint64_t> {
                  if (!v.IsUint64()) {
                    return absl::nullopt;
                  }
                  return v.Uint64OrDie();
                }) {}
};

class FakeStringMap : public FakeMap<CelValue::StringHolder> {
 public:
  explicit FakeStringMap(const std::map<CelValue::StringHolder, CelValue>& data)
      : FakeMap(
            data,
            [](CelValue::StringHolder v) { return CelValue::CreateString(v); },
            [](CelValue v) -> absl::optional<CelValue::StringHolder> {
              if (!v.IsString()) {
                return absl::nullopt;
              }
              return v.StringOrDie();
            }) {}
};

// Test list index access function
TEST_F(BuiltinsTest, ListIndex) {
  constexpr int64_t kValues[] = {3, 4, 5, 6};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateInt64(value));
  }

  FakeList cel_list(values);

  for (size_t i = 0; i < values.size(); i++) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(
        PerformRun(builtin::kIndex, {},
                   {CelValue::CreateList(&cel_list), CelValue::CreateInt64(i)},
                   &result_value));

    ASSERT_TRUE(result_value.IsInt64());
    EXPECT_THAT(result_value.Int64OrDie(), Eq(kValues[i]));
  }
}

// Test Equality/Non-Equality operation for lists
TEST_F(BuiltinsTest, TestListEqual) {
  const FakeList kList0({});
  const FakeList kList1({CelValue::CreateInt64(1), CelValue::CreateInt64(2)});
  const FakeList kList2({CelValue::CreateInt64(1), CelValue::CreateInt64(3)});
  const FakeList kList3({CelValue::CreateInt64(1), CelValue::CreateInt64(2),
                         CelValue::CreateInt64(3)});

  std::vector<CelValue> values;
  values.push_back(CelValue::CreateList(&kList0));
  values.push_back(CelValue::CreateList(&kList1));
  values.push_back(CelValue::CreateList(&kList2));
  values.push_back(CelValue::CreateList(&kList3));

  for (size_t i = 0; i < values.size(); i++) {
    for (size_t j = 0; j < values.size(); j++) {
      if (i == j) {
        TestComparison(builtin::kEqual, values[i], values[j], true);
        TestComparison(builtin::kInequal, values[i], values[j], false);
      } else {
        TestComparison(builtin::kInequal, values[i], values[j], true);
        TestComparison(builtin::kEqual, values[i], values[j], false);
      }
    }
  }

  const FakeList kList({CelValue::CreateInt64(1), CelValue::CreateBool(true)});
  TestNoMatchingEqualOverload(CelValue::CreateList(&kList1),
                              CelValue::CreateList(&kList));
}

// Test map index access function
TEST_F(BuiltinsTest, MapInt64Index) {
  constexpr int64_t kValues[] = {3, -4, 5, -6};
  std::map<int64_t, CelValue> data;
  for (auto value : kValues) {
    data[value] = CelValue::CreateInt64(value * value);
  }
  FakeInt64Map cel_map(data);
  for (int64_t value : kValues) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        builtin::kIndex, {},
        {CelValue::CreateMap(&cel_map), CelValue::CreateInt64(value)},
        &result_value));
    ASSERT_TRUE(result_value.IsInt64());
    EXPECT_THAT(result_value.Int64OrDie(), Eq(value * value));
  }

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kIndex, {},
                 {CelValue::CreateMap(&cel_map), CelValue::CreateInt64(100)},
                 &result_value));

  ASSERT_TRUE(result_value.IsError());
  EXPECT_THAT(result_value.ErrorOrDie()->code(),
              Eq(absl::StatusCode::kNotFound));
  EXPECT_TRUE(CheckNoSuchKeyError(result_value));
}

TEST_F(BuiltinsTest, MapUint64Index) {
  constexpr uint64_t kValues[] = {3, 4, 5, 6};
  std::map<uint64_t, CelValue> data;
  for (auto value : kValues) {
    data[value] = CelValue::CreateUint64(value * value);
  }
  FakeUint64Map cel_map(data);
  for (uint64_t value : kValues) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        builtin::kIndex, {},
        {CelValue::CreateMap(&cel_map), CelValue::CreateUint64(value)},
        &result_value));
    ASSERT_TRUE(result_value.IsUint64());
    EXPECT_THAT(result_value.Uint64OrDie(), Eq(value * value));
  }

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kIndex, {},
                 {CelValue::CreateMap(&cel_map), CelValue::CreateUint64(100)},
                 &result_value));

  ASSERT_TRUE(result_value.IsError());
  EXPECT_THAT(result_value.ErrorOrDie()->code(),
              Eq(absl::StatusCode::kNotFound));
  EXPECT_TRUE(CheckNoSuchKeyError(result_value));
}

TEST_F(BuiltinsTest, MapStringIndex) {
  std::vector<std::string> kValues = {"test0", "test1", "test2"};
  std::map<CelValue::StringHolder, CelValue> data;
  for (size_t i = 0; i < kValues.size(); i++) {
    data[CelValue::StringHolder(&kValues[i])] = CelValue::CreateInt64(i);
  }
  FakeStringMap cel_map(data);
  for (size_t i = 0; i < kValues.size(); i++) {
    std::string value = kValues[i];
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        builtin::kIndex, {},
        {CelValue::CreateMap(&cel_map), CelValue::CreateString(&value)},
        &result_value));
    ASSERT_TRUE(result_value.IsInt64());
    EXPECT_THAT(result_value.Int64OrDie(), Eq(i));
  }

  CelValue result_value;
  const std::string kMissingKey = "no_such_key_is_present";
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kIndex, {},
      {CelValue::CreateMap(&cel_map), CelValue::CreateString(&kMissingKey)},
      &result_value));

  ASSERT_TRUE(result_value.IsError());
  EXPECT_THAT(result_value.ErrorOrDie()->code(),
              Eq(absl::StatusCode::kNotFound));
  EXPECT_TRUE(CheckNoSuchKeyError(result_value));
}

TEST_F(BuiltinsTest, MapBoolIndex) {
  std::vector<bool> kValues = {true, false};
  std::map<bool, CelValue> data;
  for (size_t i = 0; i < kValues.size(); i++) {
    data[kValues[i]] = CelValue::CreateInt64(i);
  }
  FakeBoolMap cel_map(data);
  for (size_t i = 0; i < kValues.size(); i++) {
    bool value = kValues[i];
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(
        PerformRun(builtin::kIndex, {},
                   {CelValue::CreateMap(&cel_map), CelValue::CreateBool(value)},
                   &result_value));
    ASSERT_TRUE(result_value.IsInt64());
    EXPECT_THAT(result_value.Int64OrDie(), Eq(i));
  }
}

// Test Equality/Non-Equality operation for maps
TEST_F(BuiltinsTest, TestMapEqual) {
  const FakeInt64Map kMap0({});
  const FakeInt64Map kMap1({{0, CelValue::CreateInt64(0)}});
  const FakeInt64Map kMap2({{0, CelValue::CreateInt64(1)}});
  const FakeInt64Map kMap3(
      {{0, CelValue::CreateInt64(0)}, {1, CelValue::CreateInt64(1)}});

  std::vector<CelValue> values;
  values.push_back(CelValue::CreateMap(&kMap0));
  values.push_back(CelValue::CreateMap(&kMap1));
  values.push_back(CelValue::CreateMap(&kMap2));
  values.push_back(CelValue::CreateMap(&kMap3));

  for (size_t i = 0; i < values.size(); i++) {
    for (size_t j = 0; j < values.size(); j++) {
      if (i == j) {
        TestComparison(builtin::kEqual, values[i], values[j], true);
        TestComparison(builtin::kInequal, values[i], values[j], false);
      } else {
        TestComparison(builtin::kInequal, values[i], values[j], true);
        TestComparison(builtin::kEqual, values[i], values[j], false);
      }
    }
  }

  const FakeInt64Map kMap({{0, CelValue::CreateBool(true)}});
  TestNoMatchingEqualOverload(CelValue::CreateMap(&kMap1),
                              CelValue::CreateMap(&kMap));
}

TEST_F(BuiltinsTest, TestNestedEqual) {
  const std::string test = "testvalue";
  Duration dur;
  dur.set_seconds(2);
  dur.set_nanos(1);
  Timestamp ts;
  ts.set_seconds(100);
  ts.set_nanos(100);
  const FakeInt64Map kMap({{0, CelValue::CreateBool(true)}});

  const FakeList kList1({CelValue::CreateBool(true)});
  const FakeList kList2({CelValue::CreateInt64(12)});
  const FakeList kList3({CelValue::CreateUint64(13)});
  const FakeList kList4({CelValue::CreateDouble(14)});
  const FakeList kList5({CelValue::CreateString(&test)});
  const FakeList kList6({CelValue::CreateBytes(&test)});
  const FakeList kList7({CelValue::CreateNull()});
  const FakeList kList8({CelProtoWrapper::CreateDuration(&dur)});
  const FakeList kList9({CelProtoWrapper::CreateTimestamp(&ts)});
  const FakeList kList10({CelValue::CreateList(&kList1)});
  const FakeList kList11({CelValue::CreateMap(&kMap)});

  std::vector<CelValue> values;
  values.push_back(CelValue::CreateList(&kList1));
  values.push_back(CelValue::CreateList(&kList2));
  values.push_back(CelValue::CreateList(&kList3));
  values.push_back(CelValue::CreateList(&kList4));
  values.push_back(CelValue::CreateList(&kList5));
  values.push_back(CelValue::CreateList(&kList6));
  values.push_back(CelValue::CreateList(&kList7));
  values.push_back(CelValue::CreateList(&kList8));
  values.push_back(CelValue::CreateList(&kList9));
  values.push_back(CelValue::CreateList(&kList10));
  values.push_back(CelValue::CreateList(&kList11));

  for (size_t i = 0; i < values.size(); i++) {
    for (size_t j = 0; j < values.size(); j++) {
      if (i == j) {
        TestComparison(builtin::kEqual, values[i], values[j], true);
        TestComparison(builtin::kInequal, values[i], values[j], false);
      } else {
        TestNoMatchingEqualOverload(values[i], values[j]);
      }
    }
  }
}

TEST_F(BuiltinsTest, StringSize) {
  std::string test = "testvalue";
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kSize, {}, {CelValue::CreateString(&test)}, &result_value));

  ASSERT_EQ(result_value.IsInt64(), true);
  ASSERT_EQ(result_value.Int64OrDie(), 9);
}

TEST_F(BuiltinsTest, StringUnicodeSize) {
  std::string test = "πέντε";
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kSize, {}, {CelValue::CreateString(&test)}, &result_value));
  ASSERT_EQ(result_value.IsInt64(), true);
  ASSERT_EQ(result_value.Int64OrDie(), 5);
}

TEST_F(BuiltinsTest, BytesSize) {
  std::string test = "testvalue";
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kSize, {}, {CelValue::CreateBytes(&test)}, &result_value));

  ASSERT_EQ(result_value.IsInt64(), true);

  ASSERT_EQ(result_value.Int64OrDie(), test.size());
}

TEST_F(BuiltinsTest, ListSize) {
  constexpr int64_t kValues[] = {3, 4, 5, 6};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateInt64(value));
  }

  FakeList cel_list(values);

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kSize, {}, {CelValue::CreateList(&cel_list)}, &result_value));

  ASSERT_EQ(result_value.IsInt64(), true);

  ASSERT_EQ(result_value.Int64OrDie(), values.size());
}

TEST_F(BuiltinsTest, MapSize) {
  constexpr int64_t kValues[] = {3, -4, 5, -6};
  std::map<int64_t, CelValue> data;
  for (auto value : kValues) {
    data[value] = CelValue::CreateInt64(value * value);
  }
  FakeInt64Map cel_map(data);
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kSize, {}, {CelValue::CreateMap(&cel_map)}, &result_value));
  ASSERT_EQ(result_value.IsInt64(), true);
  ASSERT_EQ(result_value.Int64OrDie(), data.size());
}

TEST_F(BuiltinsTest, TestBoolListIn) {
  FakeList cel_list({CelValue::CreateBool(false), CelValue::CreateBool(false)});

  TestInList(&cel_list, CelValue::CreateBool(false), true);
  TestInList(&cel_list, CelValue::CreateBool(true), false);
}

TEST_F(BuiltinsTest, TestInt64ListIn) {
  FakeList cel_list({CelValue::CreateInt64(1), CelValue::CreateInt64(2)});

  TestInList(&cel_list, CelValue::CreateInt64(2), true);
  TestInList(&cel_list, CelValue::CreateInt64(3), false);
}

TEST_F(BuiltinsTest, TestUint64ListIn) {
  FakeList cel_list({CelValue::CreateUint64(1), CelValue::CreateUint64(2)});

  TestInList(&cel_list, CelValue::CreateUint64(2), true);
  TestInList(&cel_list, CelValue::CreateUint64(3), false);
}

TEST_F(BuiltinsTest, TestDoubleListIn) {
  FakeList cel_list({CelValue::CreateDouble(1), CelValue::CreateDouble(2)});

  TestInList(&cel_list, CelValue::CreateDouble(2), true);
  TestInList(&cel_list, CelValue::CreateDouble(3), false);
}

TEST_F(BuiltinsTest, TestStringListIn) {
  std::string v0 = "test0";
  std::string v1 = "test1";
  std::string v2 = "test2";

  FakeList cel_list({CelValue::CreateString(&v0), CelValue::CreateString(&v1)});

  TestInList(&cel_list, CelValue::CreateString(&v1), true);
  TestInList(&cel_list, CelValue::CreateString(&v2), false);
}

TEST_F(BuiltinsTest, TestBytesListIn) {
  std::vector<CelValue> values;

  std::string v0 = "test0";
  std::string v1 = "test1";
  std::string v2 = "test2";

  FakeList cel_list({CelValue::CreateBytes(&v0), CelValue::CreateBytes(&v1)});

  TestInList(&cel_list, CelValue::CreateBytes(&v1), true);
  TestInList(&cel_list, CelValue::CreateBytes(&v2), false);
}

TEST_F(HeterogeneousEqualityTest, MixedTypes) {
  FakeList cel_list({CelValue::CreateDuration(absl::Seconds(1)),
                     CelValue::CreateNull(), CelValue::CreateInt64(1)});

  ASSERT_NO_FATAL_FAILURE(
      TestInList(&cel_list, CelValue::CreateDuration(absl::Seconds(1)), true));
  ASSERT_NO_FATAL_FAILURE(
      TestInList(&cel_list, CelValue::CreateInt64(1), true));

  ASSERT_NO_FATAL_FAILURE(
      TestInList(&cel_list, CelValue::CreateUint64(1), true));

  ASSERT_NO_FATAL_FAILURE(
      TestInList(&cel_list, CelValue::CreateInt64(2), false));
  ASSERT_NO_FATAL_FAILURE(
      TestInList(&cel_list, CelValue::CreateStringView("abc"), false));
}

TEST_F(HeterogeneousEqualityTest, NullIn) {
  FakeList cel_list({CelValue::CreateInt64(0), CelValue::CreateNull(),
                     CelValue::CreateInt64(1)});

  ASSERT_NO_FATAL_FAILURE(
      TestInList(&cel_list, CelValue::CreateInt64(1), true));
  ASSERT_NO_FATAL_FAILURE(TestInList(&cel_list, CelValue::CreateNull(), true));
  ASSERT_NO_FATAL_FAILURE(
      TestInList(&cel_list, CelValue::CreateInt64(2), false));
}

TEST_F(HeterogeneousEqualityTest, NullNotIn) {
  FakeList cel_list({CelValue::CreateInt64(0), CelValue::CreateInt64(1)});

  ASSERT_NO_FATAL_FAILURE(TestInList(&cel_list, CelValue::CreateNull(), false));
}

TEST_F(BuiltinsTest, TestMapInError) {
  FakeErrorMap cel_map;
  std::vector<CelValue> kValues = {
      CelValue::CreateBool(true),
      CelValue::CreateInt64(1),
      CelValue::CreateStringView("hello"),
      CelValue::CreateUint64(2),
  };

  options_.enable_heterogeneous_equality = true;
  for (auto key : kValues) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        builtin::kIn, {}, {key, CelValue::CreateMap(&cel_map)}, &result_value));
    EXPECT_TRUE(result_value.IsBool());
    EXPECT_FALSE(result_value.BoolOrDie());
  }

  options_.enable_heterogeneous_equality = false;
  for (auto key : kValues) {
    CelValue result_value;
    ASSERT_NO_FATAL_FAILURE(PerformRun(
        builtin::kIn, {}, {key, CelValue::CreateMap(&cel_map)}, &result_value));

    EXPECT_TRUE(result_value.IsError());
    EXPECT_EQ(result_value.ErrorOrDie()->message(), "bad key type");
    EXPECT_EQ(result_value.ErrorOrDie()->code(),
              absl::StatusCode::kInvalidArgument);
  }
}

TEST_F(BuiltinsTest, TestBoolMapIn) {
  constexpr bool kValues[] = {true, true};
  std::map<bool, CelValue> data;
  for (auto value : kValues) {
    data[value] = CelValue::CreateBool(value);
  }
  FakeBoolMap cel_map(data);
  TestInMap(&cel_map, CelValue::CreateBool(true), true);
  TestInMap(&cel_map, CelValue::CreateBool(false), false);
  TestInMap(&cel_map, CelValue::CreateUint64(3), false);
}

TEST_F(BuiltinsTest, TestInt64MapIn) {
  constexpr int64_t kValues[] = {3, -4, 5, -6};
  std::map<int64_t, CelValue> data;
  for (auto value : kValues) {
    data[value] = CelValue::CreateInt64(value * value);
  }
  FakeInt64Map cel_map(data);
  options_.enable_heterogeneous_equality = false;
  TestInMap(&cel_map, CelValue::CreateInt64(-4), true);
  TestInMap(&cel_map, CelValue::CreateInt64(4), false);
  TestInMap(&cel_map, CelValue::CreateUint64(3), false);
  TestInMap(&cel_map, CelValue::CreateUint64(4), false);

  options_.enable_heterogeneous_equality = true;
  TestInMap(&cel_map, CelValue::CreateUint64(3), true);
  TestInMap(&cel_map, CelValue::CreateUint64(4), false);
  TestInMap(&cel_map, CelValue::CreateDouble(NAN), false);
  TestInMap(&cel_map, CelValue::CreateDouble(-4.0), true);
  TestInMap(&cel_map, CelValue::CreateDouble(-4.1), false);
  TestInMap(&cel_map,
            CelValue::CreateDouble(std::numeric_limits<uint64_t>::max()),
            false);
}

TEST_F(BuiltinsTest, TestUint64MapIn) {
  constexpr uint64_t kValues[] = {3, 4, 5, 6};
  std::map<uint64_t, CelValue> data;
  for (auto value : kValues) {
    data[value] = CelValue::CreateUint64(value * value);
  }
  FakeUint64Map cel_map(data);
  options_.enable_heterogeneous_equality = false;
  TestInMap(&cel_map, CelValue::CreateUint64(4), true);
  TestInMap(&cel_map, CelValue::CreateUint64(44), false);
  TestInMap(&cel_map, CelValue::CreateInt64(4), false);

  options_.enable_heterogeneous_equality = true;
  TestInMap(&cel_map, CelValue::CreateInt64(-1), false);
  TestInMap(&cel_map, CelValue::CreateInt64(4), true);
  TestInMap(&cel_map, CelValue::CreateDouble(4.0), true);
  TestInMap(&cel_map, CelValue::CreateDouble(-4.0), false);
  TestInMap(&cel_map, CelValue::CreateDouble(7.0), false);
}

TEST_F(BuiltinsTest, TestStringMapIn) {
  std::vector<std::string> kValues = {"test0", "test1", "test2", "42"};
  std::map<CelValue::StringHolder, CelValue> data;
  for (size_t i = 0; i < kValues.size(); i++) {
    data[CelValue::StringHolder(&kValues[i])] = CelValue::CreateInt64(i);
  }
  FakeStringMap cel_map(data);
  TestInMap(&cel_map, CelValue::CreateString(&kValues[0]), true);
  TestInMap(&cel_map, CelValue::CreateString(&kValues[3]), true);
  TestInMap(&cel_map, CelValue::CreateInt64(42), false);
}

TEST_F(BuiltinsTest, TestInt64Arithmetics) {
  TestArithmeticalOperationInt64(builtin::kAdd, 2, 3, 5);
  TestArithmeticalOperationInt64(builtin::kSubtract, 2, 3, -1);
  TestArithmeticalOperationInt64(builtin::kMultiply, 2, 3, 6);
  TestArithmeticalOperationInt64(builtin::kDivide, 10, 5, 2);
}

TEST_F(BuiltinsTest, TestInt64ArithmeticOverflow) {
  int64_t min = std::numeric_limits<int64_t>::lowest();
  int64_t max = std::numeric_limits<int64_t>::max();
  TestArithmeticalErrorInt64(builtin::kAdd, max, 1,
                             absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorInt64(builtin::kSubtract, min, max,
                             absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorInt64(builtin::kMultiply, max, 2,
                             absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorInt64(builtin::kModulo, min, -1,
                             absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorInt64(builtin::kDivide, min, -1,
                             absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorInt64(builtin::kDivide, min, 0,
                             absl::StatusCode::kInvalidArgument);
}

TEST_F(BuiltinsTest, TestUint64Arithmetics) {
  TestArithmeticalOperationUint64(builtin::kAdd, 2, 3, 5);
  TestArithmeticalOperationUint64(builtin::kSubtract, 3, 2, 1);
  TestArithmeticalOperationUint64(builtin::kMultiply, 2, 3, 6);
  TestArithmeticalOperationUint64(builtin::kDivide, 10, 5, 2);
}

TEST_F(BuiltinsTest, TestUint64ArithmeticOverflow) {
  CelValue result_value;
  uint64_t max = std::numeric_limits<uint64_t>::max();
  TestArithmeticalErrorUint64(builtin::kAdd, max, 1,
                              absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorUint64(builtin::kSubtract, 2, 3,
                              absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorUint64(builtin::kMultiply, max, 2,
                              absl::StatusCode::kOutOfRange);
  TestArithmeticalErrorUint64(builtin::kDivide, 1, 0,
                              absl::StatusCode::kInvalidArgument);
}

TEST_F(BuiltinsTest, TestDoubleArithmetics) {
  TestArithmeticalOperationDouble(builtin::kAdd, 2.5, 3, 5.5);
  TestArithmeticalOperationDouble(builtin::kSubtract, 2.9, 3.9, -1.);
  TestArithmeticalOperationDouble(builtin::kMultiply, 2, 3, 6);
  TestArithmeticalOperationDouble(builtin::kDivide, 1.44, 1.2, 1.2);
}

TEST_F(BuiltinsTest, TestDoubleDivisionByZero) {
  CelValue result_value;

  ASSERT_NO_FATAL_FAILURE(PerformRun(
      builtin::kDivide, {},
      {CelValue::CreateDouble(1), CelValue::CreateDouble(0)}, &result_value));
  ASSERT_TRUE(result_value.IsDouble());
  ASSERT_EQ(result_value.DoubleOrDie(),
            std::numeric_limits<double>::infinity());
}

// Test Concatenation operation for string
TEST_F(BuiltinsTest, TestConcatString) {
  const std::string kString1 = "t1";
  const std::string kString2 = "t2";

  std::vector<CelValue> args = {CelValue::CreateString(&kString1),
                                CelValue::CreateString(&kString2)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kAdd, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsString());
  EXPECT_EQ(result_value.StringOrDie().value(), kString1 + kString2);
}

// Test Concatenation operation for Bytes
TEST_F(BuiltinsTest, TestConcatBytes) {
  const std::string kBytes1 = "t1";
  const std::string kBytes2 = "t2";

  std::vector<CelValue> args = {CelValue::CreateBytes(&kBytes1),
                                CelValue::CreateBytes(&kBytes2)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kAdd, {}, args, &result_value));

  ASSERT_TRUE(result_value.IsBytes());
  EXPECT_EQ(result_value.BytesOrDie().value(), kBytes1 + kBytes2);
}

// Test Concatenation operation for CelList
TEST_F(BuiltinsTest, TestConcatList) {
  const std::vector<int> kValues({5, 6, 7, 8});

  const FakeList kList1(
      {CelValue::CreateInt64(kValues[0]), CelValue::CreateInt64(kValues[1])});
  const FakeList kList2(
      {CelValue::CreateInt64(kValues[2]), CelValue::CreateInt64(kValues[3])});

  std::vector<CelValue> args = {CelValue::CreateList(&kList1),
                                CelValue::CreateList(&kList2)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kAdd, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsList());

  const CelList* result_list = result_value.ListOrDie();
  ASSERT_EQ(result_list->size(), kValues.size());

  for (int i = 0; i < result_list->size(); i++) {
    CelValue item = (*result_list)[i];
    ASSERT_TRUE(item.IsInt64());
    EXPECT_EQ(item.Int64OrDie(), kValues[i]);
  }
}

TEST_F(BuiltinsTest, MatchesPartialTrue) {
  std::string target = "haystack";
  std::string regex = "\\w{2}ack";
  std::vector<CelValue> args = {CelValue::CreateString(&target),
                                CelValue::CreateString(&regex)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kRegexMatch, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsBool());
  EXPECT_TRUE(result_value.BoolOrDie());
}

TEST_F(BuiltinsTest, MatchesPartialFalse) {
  std::string target = "haystack";
  std::string regex = "hy";
  std::vector<CelValue> args = {CelValue::CreateString(&target),
                                CelValue::CreateString(&regex)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kRegexMatch, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsBool());
  EXPECT_FALSE(result_value.BoolOrDie());
}

TEST_F(BuiltinsTest, MatchesPartialError) {
  std::string target = "haystack";
  std::string invalid_regex = "(";
  std::vector<CelValue> args = {CelValue::CreateString(&target),
                                CelValue::CreateString(&invalid_regex)};

  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kRegexMatch, {}, args, &result_value));
  EXPECT_TRUE(result_value.IsError());
}

TEST_F(BuiltinsTest, MatchesMaxSize) {
  std::string target = "haystack";
  std::string large_regex = "[hj][ab][yt][st][tv][ac]";
  std::vector<CelValue> args = {CelValue::CreateString(&target),
                                CelValue::CreateString(&large_regex)};

  CelValue result_value;
  InterpreterOptions options;
  options.regex_max_program_size = 1;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kRegexMatch, {}, args, &result_value, options));
  EXPECT_TRUE(result_value.IsError());
}

TEST_F(BuiltinsTest, StringToIntNonInt) {
  std::string target = "not_a_number";
  std::vector<CelValue> args = {CelValue::CreateString(&target)};
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(PerformRun(builtin::kInt, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsError());
}

TEST_F(BuiltinsTest, IntToString) {
  std::vector<CelValue> args = {CelValue::CreateInt64(-42)};
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kString, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsString());
  EXPECT_EQ(result_value.StringOrDie().value(), "-42");
}

TEST_F(BuiltinsTest, UIntToString) {
  std::vector<CelValue> args = {CelValue::CreateUint64(42)};
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kString, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsString());
  EXPECT_EQ(result_value.StringOrDie().value(), "42");
}

TEST_F(BuiltinsTest, DoubleToString) {
  std::vector<CelValue> args = {CelValue::CreateDouble(37.5)};
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kString, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsString());
  EXPECT_EQ(result_value.StringOrDie().value(), "37.5");
}

TEST_F(BuiltinsTest, BytesToString) {
  std::string input = "abcd";
  std::vector<CelValue> args = {CelValue::CreateBytes(&input)};
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kString, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsString());
  EXPECT_EQ(result_value.StringOrDie().value(), "abcd");
}

TEST_F(BuiltinsTest, BytesToStringInvalid) {
  std::string input = "\xFF";
  std::vector<CelValue> args = {CelValue::CreateBytes(&input)};
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kString, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsError());
}

TEST_F(BuiltinsTest, StringToString) {
  std::string input = "abcd";
  std::vector<CelValue> args = {CelValue::CreateString(&input)};
  CelValue result_value;
  ASSERT_NO_FATAL_FAILURE(
      PerformRun(builtin::kString, {}, args, &result_value));
  ASSERT_TRUE(result_value.IsString());
  EXPECT_EQ(result_value.StringOrDie().value(), "abcd");
}

// Type operations
TEST_F(BuiltinsTest, TypeComparisons) {
  std::vector<std::pair<CelValue, CelValue>> paired_values;

  paired_values.push_back(
      {CelValue::CreateBool(false), CelValue::CreateBool(true)});
  paired_values.push_back(
      {CelValue::CreateInt64(-1), CelValue::CreateInt64(1)});
  paired_values.push_back(
      {CelValue::CreateUint64(1), CelValue::CreateUint64(2)});
  paired_values.push_back(
      {CelValue::CreateDouble(1.), CelValue::CreateDouble(2.)});

  std::string str1 = "test1";
  std::string str2 = "test2";
  paired_values.push_back(
      {CelValue::CreateString(&str1), CelValue::CreateString(&str2)});
  paired_values.push_back(
      {CelValue::CreateBytes(&str1), CelValue::CreateBytes(&str2)});

  FakeList cel_list1({CelValue::CreateBool(false)});
  FakeList cel_list2({CelValue::CreateBool(true)});
  paired_values.push_back(
      {CelValue::CreateList(&cel_list1), CelValue::CreateList(&cel_list2)});

  std::map<int64_t, CelValue> data1;
  std::map<int64_t, CelValue> data2;
  FakeInt64Map cel_map1(data1);
  FakeInt64Map cel_map2(data2);
  paired_values.push_back(
      {CelValue::CreateMap(&cel_map1), CelValue::CreateMap(&cel_map2)});

  for (size_t i = 0; i < paired_values.size(); i++) {
    for (size_t j = 0; j < paired_values.size(); j++) {
      CelValue result1;
      CelValue result2;

      PerformRun(builtin::kType, {}, {paired_values[i].first}, &result1);
      PerformRun(builtin::kType, {}, {paired_values[j].second}, &result2);

      ASSERT_TRUE(result1.IsCelType()) << "Unexpected result for value at index"
                                       << i << ":" << result1.DebugString();
      ASSERT_TRUE(result2.IsCelType()) << "Unexpected result for value at index"
                                       << j << ":" << result2.DebugString();
      if (i == j) {
        ASSERT_EQ(result1.CelTypeOrDie(), result2.CelTypeOrDie());
      } else {
        ASSERT_TRUE(result1.CelTypeOrDie() != result2.CelTypeOrDie());
      }
    }
  }
}

}  // namespace
}  // namespace google::api::expr::runtime
