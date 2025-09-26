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
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "runtime/function.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

using FunctionAdapterTest = common_internal::ValueTest<>;

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionInt) {
  using FunctionAdapter = UnaryFunctionAdapter<int64_t, int64_t>;

  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](int64_t x) -> int64_t { return x + 2; });

  std::vector<Value> args{IntValue(40)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.GetInt().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = UnaryFunctionAdapter<double, double>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](double x) -> double { return x * 2; });

  std::vector<Value> args{DoubleValue(40.0)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<DoubleValue>());
  EXPECT_EQ(result.GetDouble().NativeValue(), 80.0);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](uint64_t x) -> uint64_t { return x - 2; });

  std::vector<Value> args{UintValue(44)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionBool) {
  using FunctionAdapter = UnaryFunctionAdapter<bool, bool>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](bool x) -> bool { return !x; });

  std::vector<Value> args{BoolValue(true)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<BoolValue>());
  EXPECT_EQ(result.GetBool().NativeValue(), false);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionTimestamp) {
  using FunctionAdapter = UnaryFunctionAdapter<absl::Time, absl::Time>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](absl::Time x) -> absl::Time { return x + absl::Minutes(1); });

  std::vector<Value> args;
  args.emplace_back() = TimestampValue(absl::UnixEpoch());
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<TimestampValue>());
  EXPECT_EQ(result.GetTimestamp().NativeValue(),
            absl::UnixEpoch() + absl::Minutes(1));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionDuration) {
  using FunctionAdapter = UnaryFunctionAdapter<absl::Duration, absl::Duration>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](absl::Duration x) -> absl::Duration { return x + absl::Seconds(2); });

  std::vector<Value> args;
  args.emplace_back() = DurationValue(absl::Seconds(6));
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<DurationValue>());
  EXPECT_EQ(result.GetDuration().NativeValue(), absl::Seconds(8));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionString) {
  using FunctionAdapter = UnaryFunctionAdapter<StringValue, StringValue>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](const StringValue& x) -> StringValue {
        return StringValue("pre_" + x.ToString());
      });

  std::vector<Value> args;
  args.emplace_back() = StringValue("string");
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "pre_string");
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionBytes) {
  using FunctionAdapter = UnaryFunctionAdapter<BytesValue, BytesValue>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](const BytesValue& x) -> BytesValue {
        return BytesValue("pre_" + x.ToString());
      });

  std::vector<Value> args;
  args.emplace_back() = BytesValue("bytes");
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<BytesValue>());
  EXPECT_EQ(result.GetBytes().ToString(), "pre_bytes");
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, Value>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](const Value& x) -> uint64_t { return x.GetUint().NativeValue() - 2; });

  std::vector<Value> args{UintValue(44)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionReturnError) {
  using FunctionAdapter = UnaryFunctionAdapter<Value, uint64_t>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](uint64_t x) -> Value {
        return ErrorValue(absl::InvalidArgumentError("test_error"));
      });

  std::vector<Value> args{UintValue(44)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<ErrorValue>());
  EXPECT_THAT(result.GetError().NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument, "test_error"));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionPropagateStatus) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](uint64_t x) -> absl::StatusOr<uint64_t> {
        // Returning a status directly stops CEL evaluation and
        // immediately returns.
        return absl::InternalError("test_error");
      });

  std::vector<Value> args{UintValue(44)};
  EXPECT_THAT(
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(FunctionAdapterTest,
       UnaryFunctionAdapterWrapFunctionReturnStatusOrValue) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](uint64_t x) -> absl::StatusOr<uint64_t> { return x; });

  std::vector<Value> args{UintValue(44)};
  ASSERT_OK_AND_ASSIGN(
      Value result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));
  EXPECT_EQ(result.GetUint().NativeValue(), 44);
}

TEST_F(FunctionAdapterTest,
       UnaryFunctionAdapterWrapFunctionWrongArgCountError) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](uint64_t x) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Value> args{UintValue(44), UintValue(43)};
  EXPECT_THAT(
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "unexpected number of arguments for unary function"));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionWrongArgTypeError) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](uint64_t x) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Value> args{DoubleValue(44)};
  EXPECT_THAT(
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()),
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
      [](int64_t x, int64_t y) -> int64_t { return x + y; });

  std::vector<Value> args{IntValue(21), IntValue(21)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.GetInt().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = BinaryFunctionAdapter<double, double, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](double x, double y) -> double { return x * y; });

  std::vector<Value> args{DoubleValue(40.0), DoubleValue(2.0)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<DoubleValue>());
  EXPECT_EQ(result.GetDouble().NativeValue(), 80.0);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](uint64_t x, uint64_t y) -> uint64_t { return x - y; });

  std::vector<Value> args{UintValue(44), UintValue(2)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionBool) {
  using FunctionAdapter = BinaryFunctionAdapter<bool, bool, bool>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](bool x, bool y) -> bool { return x != y; });

  std::vector<Value> args{BoolValue(false), BoolValue(true)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<BoolValue>());
  EXPECT_EQ(result.GetBool().NativeValue(), true);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionTimestamp) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::Time, absl::Time, absl::Time>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](absl::Time x, absl::Time y) -> absl::Time { return x > y ? x : y; });

  std::vector<Value> args;
  args.emplace_back() = TimestampValue(absl::UnixEpoch() + absl::Seconds(1));
  args.emplace_back() = TimestampValue(absl::UnixEpoch() + absl::Seconds(2));

  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<TimestampValue>());
  EXPECT_EQ(result.GetTimestamp().NativeValue(),
            absl::UnixEpoch() + absl::Seconds(2));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionDuration) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::Duration, absl::Duration, absl::Duration>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](absl::Duration x, absl::Duration y) -> absl::Duration {
        return x > y ? x : y;
      });

  std::vector<Value> args;
  args.emplace_back() = DurationValue(absl::Seconds(5));
  args.emplace_back() = DurationValue(absl::Seconds(2));

  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<DurationValue>());
  EXPECT_EQ(result.GetDuration().NativeValue(), absl::Seconds(5));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionString) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<StringValue>, const StringValue&,
                            const StringValue&>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](const StringValue& x,
         const StringValue& y) -> absl::StatusOr<StringValue> {
        return StringValue(x.ToString() + y.ToString());
      });

  std::vector<Value> args;
  args.emplace_back() = StringValue("abc");
  args.emplace_back() = StringValue("def");

  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "abcdef");
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionBytes) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<BytesValue>, const BytesValue&,
                            const BytesValue&>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](const BytesValue& x,
         const BytesValue& y) -> absl::StatusOr<BytesValue> {
        return BytesValue(x.ToString() + y.ToString());
      });

  std::vector<Value> args;
  args.emplace_back() = BytesValue("abc");
  args.emplace_back() = BytesValue("def");

  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<BytesValue>());
  EXPECT_EQ(result.GetBytes().ToString(), "abcdef");
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter = BinaryFunctionAdapter<uint64_t, Value, Value>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](const Value& x, const Value& y) -> uint64_t {
        return x.GetUint().NativeValue() -
               static_cast<int64_t>(y.GetDouble().NativeValue());
      });

  std::vector<Value> args{UintValue(44), DoubleValue(2)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.GetUint().NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionReturnError) {
  using FunctionAdapter = BinaryFunctionAdapter<Value, int64_t, uint64_t>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](int64_t x, uint64_t y) -> Value {
        return ErrorValue(absl::InvalidArgumentError("test_error"));
      });

  std::vector<Value> args{IntValue(44), UintValue(44)};
  ASSERT_OK_AND_ASSIGN(
      auto result,
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()));

  ASSERT_TRUE(result->Is<ErrorValue>());
  EXPECT_THAT(result.GetError().NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument, "test_error"));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionPropagateStatus) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, int64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](int64_t, uint64_t x) -> absl::StatusOr<uint64_t> {
        // Returning a status directly stops CEL evaluation and
        // immediately returns.
        return absl::InternalError("test_error");
      });

  std::vector<Value> args{IntValue(43), UintValue(44)};
  EXPECT_THAT(
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(FunctionAdapterTest,
       BinaryFunctionAdapterWrapFunctionWrongArgCountError) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](uint64_t x, double y) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Value> args{UintValue(44)};
  EXPECT_THAT(
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "unexpected number of arguments for binary function"));
}

TEST_F(FunctionAdapterTest,
       BinaryFunctionAdapterWrapFunctionWrongArgTypeError) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](int64_t x, int64_t y) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Value> args{DoubleValue(44), DoubleValue(44)};
  EXPECT_THAT(
      wrapped->Invoke(args, descriptor_pool(), message_factory(), arena()),
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
      NullaryFunctionAdapter<absl::StatusOr<Value>>::CreateDescriptor(
          "ZeroArgs", false);

  EXPECT_EQ(desc.name(), "ZeroArgs");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), IsEmpty());
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterWrapFunction0Args) {
  std::unique_ptr<Function> fn =
      NullaryFunctionAdapter<absl::StatusOr<Value>>::WrapFunction(
          []() { return StringValue("abc"); });

  ASSERT_OK_AND_ASSIGN(auto result, fn->Invoke({}, descriptor_pool(),
                                               message_factory(), arena()));
  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "abc");
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterCreateDescriptor3Args) {
  FunctionDescriptor desc = TernaryFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::CreateDescriptor("MyFormatter", false);

  EXPECT_EQ(desc.name(), "MyFormatter");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(),
              ElementsAre(Kind::kInt64, Kind::kBool, Kind::kString));
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterWrapFunction3Args) {
  std::unique_ptr<Function> fn = TernaryFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::WrapFunction([](int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Value> {
    return StringValue(absl::StrCat(int_val, "_", (bool_val ? "true" : "false"),
                                    "_", string_val.ToString()));
  });

  std::vector<Value> args{IntValue(42), BoolValue(false)};
  args.emplace_back() = StringValue("abcd");
  ASSERT_OK_AND_ASSIGN(auto result, fn->Invoke(args, descriptor_pool(),
                                               message_factory(), arena()));
  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.GetString().ToString(), "42_false_abcd");
}

TEST_F(FunctionAdapterTest,
       VariadicFunctionAdapterWrapFunction3ArgsBadArgType) {
  std::unique_ptr<Function> fn = TernaryFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::WrapFunction([](int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Value> {
    return StringValue(absl::StrCat(int_val, "_", (bool_val ? "true" : "false"),
                                    "_", string_val.ToString()));
  });

  std::vector<Value> args{IntValue(42), BoolValue(false)};
  args.emplace_back() = TimestampValue(absl::UnixEpoch());
  EXPECT_THAT(fn->Invoke(args, descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected string value")));
}

TEST_F(FunctionAdapterTest,
       VariadicFunctionAdapterWrapFunction3ArgsBadArgCount) {
  std::unique_ptr<Function> fn = TernaryFunctionAdapter<
      absl::StatusOr<Value>, int64_t, bool,
      const StringValue&>::WrapFunction([](int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Value> {
    return StringValue(absl::StrCat(int_val, "_", (bool_val ? "true" : "false"),
                                    "_", string_val.ToString()));
  });

  std::vector<Value> args{IntValue(42), BoolValue(false)};
  EXPECT_THAT(fn->Invoke(args, descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("unexpected number of arguments")));
}

}  // namespace
}  // namespace cel
