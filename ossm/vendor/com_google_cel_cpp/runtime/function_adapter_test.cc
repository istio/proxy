// Copyright 2023 Google LLC
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

#include "runtime/function_adapter.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "common/kind.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/legacy_type_reflector.h"
#include "common/values/legacy_value_manager.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

class FunctionAdapterTest : public ::testing::Test {
 public:
  FunctionAdapterTest()
      : type_reflector_(),
        value_manager_(MemoryManagerRef::ReferenceCounting(), type_reflector_),
        test_context_(value_manager_) {}

  ValueManager& value_factory() { return value_manager_; }

  const FunctionEvaluationContext& test_context() { return test_context_; }

 private:
  common_internal::LegacyTypeReflector type_reflector_;
  common_internal::LegacyValueManager value_manager_;
  FunctionEvaluationContext test_context_;
};

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionInt) {
  using FunctionAdapter = UnaryFunctionAdapter<int64_t, int64_t>;

  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, int64_t x) -> int64_t { return x + 2; });

  std::vector<Value> args{value_factory().CreateIntValue(40)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.GetInt().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = UnaryFunctionAdapter<double, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, double x) -> double { return x * 2; });

  std::vector<Value> args{value_factory().CreateDoubleValue(40.0)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DoubleValue>());
  EXPECT_EQ(result.GetDouble().NativeValue(), 80.0);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, uint64_t x) -> uint64_t { return x - 2; });

  std::vector<Value> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionBool) {
  using FunctionAdapter = UnaryFunctionAdapter<bool, bool>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, bool x) -> bool { return !x; });

  std::vector<Value> args{value_factory().CreateBoolValue(true)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BoolValue>());
  EXPECT_EQ(result.GetBool().NativeValue(), false);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionTimestamp) {
  using FunctionAdapter = UnaryFunctionAdapter<absl::Time, absl::Time>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, absl::Time x) -> absl::Time {
        return x + absl::Minutes(1);
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch()));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<TimestampValue>());
  EXPECT_EQ(result.GetTimestamp().NativeValue(),
            absl::UnixEpoch() + absl::Minutes(1));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionDuration) {
  using FunctionAdapter = UnaryFunctionAdapter<absl::Duration, absl::Duration>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, absl::Duration x) -> absl::Duration {
        return x + absl::Seconds(2);
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateDurationValue(absl::Seconds(6)));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DurationValue>());
  EXPECT_EQ(result.GetDuration().NativeValue(), absl::Seconds(8));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionString) {
  using FunctionAdapter = UnaryFunctionAdapter<StringValue, StringValue>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, const StringValue& x) -> StringValue {
        return value_factory.CreateStringValue("pre_" + x.ToString()).value();
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("string"));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "pre_string");
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionBytes) {
  using FunctionAdapter = UnaryFunctionAdapter<BytesValue, BytesValue>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, const BytesValue& x) -> BytesValue {
        return value_factory.CreateBytesValue("pre_" + x.ToString()).value();
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateBytesValue("bytes"));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BytesValue>());
  EXPECT_EQ(result.GetBytes().ToString(), "pre_bytes");
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, Value>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, const Value& x) -> uint64_t {
        return x.GetUint().NativeValue() - 2;
      });

  std::vector<Value> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionReturnError) {
  using FunctionAdapter = UnaryFunctionAdapter<Value, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, uint64_t x) -> Value {
        return value_factory.CreateErrorValue(
            absl::InvalidArgumentError("test_error"));
      });

  std::vector<Value> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<ErrorValue>());
  EXPECT_THAT(result.GetError().NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument, "test_error"));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionPropagateStatus) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        // Returning a status directly stops CEL evaluation and
        // immediately returns.
        return absl::InternalError("test_error");
      });

  std::vector<Value> args{value_factory().CreateUintValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(FunctionAdapterTest,
       UnaryFunctionAdapterWrapFunctionReturnStatusOrValue) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        return x;
      });

  std::vector<Value> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(Value result, wrapped->Invoke(test_context(), args));
  EXPECT_EQ(result.GetUint().NativeValue(), 44);
}

TEST_F(FunctionAdapterTest,
       UnaryFunctionAdapterWrapFunctionWrongArgCountError) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        return 42;
      });

  std::vector<Value> args{value_factory().CreateUintValue(44),
                          value_factory().CreateUintValue(43)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "unexpected number of arguments for unary function"));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionWrongArgTypeError) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        return 42;
      });

  std::vector<Value> args{value_factory().CreateDoubleValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected uint value")));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorInt) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, int64_t>::CreateDescriptor(
          "Increment", false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kInt64));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorDouble) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, double>::CreateDescriptor(
          "Mult2", true);

  EXPECT_EQ(desc.name(), "Mult2");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_TRUE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDouble));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorUint) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, uint64_t>::CreateDescriptor(
          "Increment", false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kUint64));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorBool) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, bool>::CreateDescriptor(
          "Not", false);

  EXPECT_EQ(desc.name(), "Not");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBool));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorTimestamp) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, absl::Time>::CreateDescriptor(
          "AddMinute", false);

  EXPECT_EQ(desc.name(), "AddMinute");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kTimestamp));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorDuration) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>,
                           absl::Duration>::CreateDescriptor("AddFiveSeconds",
                                                             false);

  EXPECT_EQ(desc.name(), "AddFiveSeconds");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDuration));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorString) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>,
                           StringValue>::CreateDescriptor("Prepend", false);

  EXPECT_EQ(desc.name(), "Prepend");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kString));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorBytes) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, BytesValue>::CreateDescriptor(
          "Prepend", false);

  EXPECT_EQ(desc.name(), "Prepend");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBytes));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorAny) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, Value>::CreateDescriptor(
          "Increment", false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kAny));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorNonStrict) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Value>, Value>::CreateDescriptor(
          "Increment", false,
          /*is_strict=*/false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_FALSE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kAny));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionInt) {
  using FunctionAdapter = BinaryFunctionAdapter<int64_t, int64_t, int64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, int64_t x, int64_t y) -> int64_t { return x + y; });

  std::vector<Value> args{value_factory().CreateIntValue(21),
                          value_factory().CreateIntValue(21)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.GetInt().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = BinaryFunctionAdapter<double, double, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, double x, double y) -> double { return x * y; });

  std::vector<Value> args{value_factory().CreateDoubleValue(40.0),
                          value_factory().CreateDoubleValue(2.0)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DoubleValue>());
  EXPECT_EQ(result.GetDouble().NativeValue(), 80.0);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, uint64_t x, uint64_t y) -> uint64_t { return x - y; });

  std::vector<Value> args{value_factory().CreateUintValue(44),
                          value_factory().CreateUintValue(2)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionBool) {
  using FunctionAdapter = BinaryFunctionAdapter<bool, bool, bool>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, bool x, bool y) -> bool { return x != y; });

  std::vector<Value> args{value_factory().CreateBoolValue(false),
                          value_factory().CreateBoolValue(true)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BoolValue>());
  EXPECT_EQ(result.GetBool().NativeValue(), true);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionTimestamp) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::Time, absl::Time, absl::Time>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, absl::Time x, absl::Time y) -> absl::Time {
        return x > y ? x : y;
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch() +
                                                            absl::Seconds(1)));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch() +
                                                            absl::Seconds(2)));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<TimestampValue>());
  EXPECT_EQ(result.GetTimestamp().NativeValue(),
            absl::UnixEpoch() + absl::Seconds(2));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionDuration) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::Duration, absl::Duration, absl::Duration>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, absl::Duration x, absl::Duration y) -> absl::Duration {
        return x > y ? x : y;
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateDurationValue(absl::Seconds(5)));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateDurationValue(absl::Seconds(2)));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DurationValue>());
  EXPECT_EQ(result.GetDuration().NativeValue(), absl::Seconds(5));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionString) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<StringValue>, const StringValue&,
                            const StringValue&>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, const StringValue& x,
         const StringValue& y) -> absl::StatusOr<StringValue> {
        return value_factory.CreateStringValue(x.ToString() + y.ToString());
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("abc"));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("def"));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "abcdef");
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionBytes) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<BytesValue>, const BytesValue&,
                            const BytesValue&>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, const BytesValue& x,
         const BytesValue& y) -> absl::StatusOr<BytesValue> {
        return value_factory.CreateBytesValue(x.ToString() + y.ToString());
      });

  std::vector<Value> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateBytesValue("abc"));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateBytesValue("def"));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BytesValue>());
  EXPECT_EQ(result.GetBytes().ToString(), "abcdef");
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter = BinaryFunctionAdapter<uint64_t, Value, Value>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager&, const Value& x, const Value& y) -> uint64_t {
        return x.GetUint().NativeValue() -
               static_cast<int64_t>(y.GetDouble().NativeValue());
      });

  std::vector<Value> args{value_factory().CreateUintValue(44),
                          value_factory().CreateDoubleValue(2)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionReturnError) {
  using FunctionAdapter = BinaryFunctionAdapter<Value, int64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, int64_t x, uint64_t y) -> Value {
        return value_factory.CreateErrorValue(
            absl::InvalidArgumentError("test_error"));
      });

  std::vector<Value> args{value_factory().CreateIntValue(44),
                          value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<ErrorValue>());
  EXPECT_THAT(result.GetError().NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument, "test_error"));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionPropagateStatus) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, int64_t, uint64_t>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](ValueManager& value_factory, int64_t,
                                       uint64_t x) -> absl::StatusOr<uint64_t> {
        // Returning a status directly stops CEL evaluation and
        // immediately returns.
        return absl::InternalError("test_error");
      });

  std::vector<Value> args{value_factory().CreateIntValue(43),
                          value_factory().CreateUintValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(FunctionAdapterTest,
       BinaryFunctionAdapterWrapFunctionWrongArgCountError) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, uint64_t x,
         double y) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Value> args{value_factory().CreateUintValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "unexpected number of arguments for binary function"));
}

TEST_F(FunctionAdapterTest,
       BinaryFunctionAdapterWrapFunctionWrongArgTypeError) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueManager& value_factory, int64_t x,
         int64_t y) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Value> args{value_factory().CreateDoubleValue(44),
                          value_factory().CreateDoubleValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected uint value")));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorInt) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, int64_t,
                            int64_t>::CreateDescriptor("Add", false);

  EXPECT_EQ(desc.name(), "Add");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kInt64, Kind::kInt64));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorDouble) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, double,
                            double>::CreateDescriptor("Mult", true);

  EXPECT_EQ(desc.name(), "Mult");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_TRUE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDouble, Kind::kDouble));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorUint) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, uint64_t,
                            uint64_t>::CreateDescriptor("Add", false);

  EXPECT_EQ(desc.name(), "Add");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kUint64, Kind::kUint64));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorBool) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, bool,
                            bool>::CreateDescriptor("Xor", false);

  EXPECT_EQ(desc.name(), "Xor");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBool, Kind::kBool));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorTimestamp) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Time,
                            absl::Time>::CreateDescriptor("Max", false);

  EXPECT_EQ(desc.name(), "Max");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kTimestamp, Kind::kTimestamp));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorDuration) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, absl::Duration,
                            absl::Duration>::CreateDescriptor("Max", false);

  EXPECT_EQ(desc.name(), "Max");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDuration, Kind::kDuration));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorString) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, StringValue,
                            StringValue>::CreateDescriptor("Concat", false);

  EXPECT_EQ(desc.name(), "Concat");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kString, Kind::kString));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorBytes) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, BytesValue,
                            BytesValue>::CreateDescriptor("Concat", false);

  EXPECT_EQ(desc.name(), "Concat");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBytes, Kind::kBytes));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorAny) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, Value,
                            Value>::CreateDescriptor("Add", false);
  EXPECT_EQ(desc.name(), "Add");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kAny, Kind::kAny));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorNonStrict) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Value>, Value,
                            Value>::CreateDescriptor("Add", false, false);
  EXPECT_EQ(desc.name(), "Add");
  EXPECT_FALSE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kAny, Kind::kAny));
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterCreateDescriptor0Args) {
  FunctionDescriptor desc =
      VariadicFunctionAdapter<absl::StatusOr<Value>>::CreateDescriptor(
          "ZeroArgs", false);

  EXPECT_EQ(desc.name(), "ZeroArgs");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), IsEmpty());
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterWrapFunction0Args) {
  std::unique_ptr<Function> fn =
      VariadicFunctionAdapter<absl::StatusOr<Value>>::WrapFunction(
          [](ValueManager& value_factory) {
            return value_factory.CreateStringValue("abc");
          });

  ASSERT_OK_AND_ASSIGN(auto result, fn->Invoke(test_context(), {}));
  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "abc");
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterCreateDescriptor3Args) {
  FunctionDescriptor desc = VariadicFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::CreateDescriptor("MyFormatter", false);

  EXPECT_EQ(desc.name(), "MyFormatter");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(),
              ElementsAre(Kind::kInt64, Kind::kBool, Kind::kString));
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterWrapFunction3Args) {
  std::unique_ptr<Function> fn = VariadicFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::WrapFunction([](ValueManager& value_factory,
                                           int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Value> {
    return value_factory.CreateStringValue(
        absl::StrCat(int_val, "_", (bool_val ? "true" : "false"), "_",
                     string_val.ToString()));
  });

  std::vector<Value> args{value_factory().CreateIntValue(42),
                          value_factory().CreateBoolValue(false)};
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("abcd"));
  ASSERT_OK_AND_ASSIGN(auto result, fn->Invoke(test_context(), args));
  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "42_false_abcd");
}

TEST_F(FunctionAdapterTest,
       VariadicFunctionAdapterWrapFunction3ArgsBadArgType) {
  std::unique_ptr<Function> fn = VariadicFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::WrapFunction([](ValueManager& value_factory,
                                           int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Value> {
    return value_factory.CreateStringValue(
        absl::StrCat(int_val, "_", (bool_val ? "true" : "false"), "_",
                     string_val.ToString()));
  });

  std::vector<Value> args{value_factory().CreateIntValue(42),
                          value_factory().CreateBoolValue(false)};
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch()));
  EXPECT_THAT(fn->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected string value")));
}

TEST_F(FunctionAdapterTest,
       VariadicFunctionAdapterWrapFunction3ArgsBadArgCount) {
  std::unique_ptr<Function> fn = VariadicFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::WrapFunction([](ValueManager& value_factory,
                                           int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Value> {
    return value_factory.CreateStringValue(
        absl::StrCat(int_val, "_", (bool_val ? "true" : "false"), "_",
                     string_val.ToString()));
  });

  std::vector<Value> args{value_factory().CreateIntValue(42),
                          value_factory().CreateBoolValue(false)};
  EXPECT_THAT(fn->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("unexpected number of arguments")));
}

}  // namespace
}  // namespace cel
