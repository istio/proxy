// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/eval/create_struct_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/ast_internal/expr.h"
#include "base/type_provider.h"
#include "common/values/legacy_value_manager.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/proto_matchers.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::TypeProvider;
using ::cel::ast_internal::Expr;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel::internal::test::EqualsProto;
using ::google::protobuf::Arena;
using ::google::protobuf::Message;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Pointwise;

absl::StatusOr<ExecutionPath> MakeStackMachinePath(absl::string_view field) {
  ExecutionPath path;
  Expr expr0;

  auto& ident = expr0.mutable_ident_expr();
  ident.set_name("message");
  CEL_ASSIGN_OR_RETURN(auto step0, CreateIdentStep(ident, expr0.id()));

  auto step1 = CreateCreateStructStep("google.api.expr.runtime.TestMessage",
                                      {std::string(field)},
                                      /*optional_indices=*/{},

                                      /*id=*/-1);

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  return path;
}

absl::StatusOr<ExecutionPath> MakeRecursivePath(absl::string_view field) {
  ExecutionPath path;

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateDirectIdentStep("message", -1));

  auto step1 =
      CreateDirectCreateStructStep("google.api.expr.runtime.TestMessage",
                                   {std::string(field)}, std::move(deps),
                                   /*optional_indices=*/{},

                                   /*id=*/-1);

  path.push_back(std::make_unique<WrappedDirectStep>(std::move(step1), -1));

  return path;
}

// Helper method. Creates simple pipeline containing CreateStruct step that
// builds message and runs it.
absl::StatusOr<CelValue> RunExpression(absl::string_view field,
                                       const CelValue& value,
                                       google::protobuf::Arena* arena,
                                       bool enable_unknowns,
                                       bool enable_recursive_planning) {
  CelTypeRegistry type_registry;
  type_registry.RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(
          google::protobuf::DescriptorPool::generated_pool(),
          google::protobuf::MessageFactory::generated_factory()));
  auto memory_manager = ProtoMemoryManagerRef(arena);
  cel::common_internal::LegacyValueManager type_manager(
      memory_manager, type_registry.GetTypeProvider());

  CEL_ASSIGN_OR_RETURN(
      auto maybe_type,
      type_manager.FindType("google.api.expr.runtime.TestMessage"));
  if (!maybe_type.has_value()) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "missing proto message type");
  }

  cel::RuntimeOptions options;
  if (enable_unknowns) {
    options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  }
  ExecutionPath path;

  if (enable_recursive_planning) {
    CEL_ASSIGN_OR_RETURN(path, MakeRecursivePath(field));
  } else {
    CEL_ASSIGN_OR_RETURN(path, MakeStackMachinePath(field));
  }

  CelExpressionFlatImpl cel_expr(
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     type_registry.GetTypeProvider(), options));
  Activation activation;
  activation.InsertValue("message", value);

  return cel_expr.Evaluate(activation, arena);
}

void RunExpressionAndGetMessage(absl::string_view field, const CelValue& value,
                                google::protobuf::Arena* arena, TestMessage* test_msg,
                                bool enable_unknowns,
                                bool enable_recursive_planning) {
  ASSERT_OK_AND_ASSIGN(auto result,
                       RunExpression(field, value, arena, enable_unknowns,
                                     enable_recursive_planning));
  ASSERT_TRUE(result.IsMessage()) << result.DebugString();

  const Message* msg = result.MessageOrDie();
  ASSERT_THAT(msg, Not(IsNull()));

  ASSERT_EQ(msg->GetDescriptor(), TestMessage::descriptor());
  test_msg->MergeFrom(*msg);
}

void RunExpressionAndGetMessage(absl::string_view field,
                                std::vector<CelValue> values,
                                google::protobuf::Arena* arena, TestMessage* test_msg,
                                bool enable_unknowns,
                                bool enable_recursive_planning) {
  ContainerBackedListImpl cel_list(std::move(values));

  CelValue value = CelValue::CreateList(&cel_list);

  ASSERT_OK_AND_ASSIGN(auto result,
                       RunExpression(field, value, arena, enable_unknowns,
                                     enable_recursive_planning));
  ASSERT_TRUE(result.IsMessage()) << result.DebugString();

  const Message* msg = result.MessageOrDie();
  ASSERT_THAT(msg, Not(IsNull()));

  ASSERT_EQ(msg->GetDescriptor(), TestMessage::descriptor());
  test_msg->MergeFrom(*msg);
}

class CreateCreateStructStepTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  bool enable_unknowns() { return std::get<0>(GetParam()); }
  bool enable_recursive_planning() { return std::get<1>(GetParam()); }
};

TEST_P(CreateCreateStructStepTest, TestEmptyMessageCreation) {
  ExecutionPath path;
  CelTypeRegistry type_registry;
  type_registry.RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(
          google::protobuf::DescriptorPool::generated_pool(),
          google::protobuf::MessageFactory::generated_factory()));
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  cel::common_internal::LegacyValueManager type_manager(
      memory_manager, type_registry.GetTypeProvider());

  auto adapter =
      type_registry.FindTypeAdapter("google.api.expr.runtime.TestMessage");
  ASSERT_TRUE(adapter.has_value() && adapter->mutation_apis() != nullptr);

  ASSERT_OK_AND_ASSIGN(
      auto maybe_type,
      type_manager.FindType("google.api.expr.runtime.TestMessage"));
  ASSERT_TRUE(maybe_type.has_value());
  if (enable_recursive_planning()) {
    auto step =
        CreateDirectCreateStructStep("google.api.expr.runtime.TestMessage",
                                     /*fields=*/{},
                                     /*deps=*/{},
                                     /*optional_indices=*/{},
                                     /*id=*/-1);
    path.push_back(
        std::make_unique<WrappedDirectStep>(std::move(step), /*id=*/-1));
  } else {
    auto step = CreateCreateStructStep("google.api.expr.runtime.TestMessage",
                                       /*fields=*/{},
                                       /*optional_indices=*/{},
                                       /*id=*/-1);
    path.push_back(std::move(step));
  }

  cel::RuntimeOptions options;
  if (enable_unknowns(), enable_recursive_planning()) {
    options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  }
  CelExpressionFlatImpl cel_expr(
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     type_registry.GetTypeProvider(), options));
  Activation activation;

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsMessage()) << result.DebugString();
  const Message* msg = result.MessageOrDie();
  ASSERT_THAT(msg, Not(IsNull()));

  ASSERT_EQ(msg->GetDescriptor(), TestMessage::descriptor());
}

// Test message creation if unknown argument is passed
TEST(CreateCreateStructStepTest, TestMessageCreateWithUnknown) {
  Arena arena;
  TestMessage test_msg;
  UnknownSet unknown_set;

  auto eval_status =
      RunExpression("bool_value", CelValue::CreateUnknownSet(&unknown_set),
                    &arena, true, /*enable_recursive_planning=*/false);
  ASSERT_OK(eval_status);
  ASSERT_TRUE(eval_status->IsUnknownSet());
}

// Test message creation if unknown argument is passed
TEST(CreateCreateStructStepTest, TestMessageCreateWithUnknownRecursive) {
  Arena arena;
  TestMessage test_msg;
  UnknownSet unknown_set;

  auto eval_status =
      RunExpression("bool_value", CelValue::CreateUnknownSet(&unknown_set),
                    &arena, true, /*enable_recursive_planning=*/true);
  ASSERT_OK(eval_status);
  ASSERT_TRUE(eval_status->IsUnknownSet()) << eval_status->DebugString();
}

// Test that fields of type bool are set correctly
TEST_P(CreateCreateStructStepTest, TestSetBoolField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bool_value", CelValue::CreateBool(true), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));
  ASSERT_EQ(test_msg.bool_value(), true);
}

// Test that fields of type int32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetInt32Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int32_value", CelValue::CreateInt64(1), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  ASSERT_EQ(test_msg.int32_value(), 1);
}

// Test that fields of type uint32_t are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetUInt32Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint32_value", CelValue::CreateUint64(1), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  ASSERT_EQ(test_msg.uint32_value(), 1);
}

// Test that fields of type int64_t are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetInt64Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int64_value", CelValue::CreateInt64(1), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  EXPECT_EQ(test_msg.int64_value(), 1);
}

// Test that fields of type uint64_t are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetUInt64Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint64_value", CelValue::CreateUint64(1), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  EXPECT_EQ(test_msg.uint64_value(), 1);
}

// Test that fields of type float are set correctly
TEST_P(CreateCreateStructStepTest, TestSetFloatField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "float_value", CelValue::CreateDouble(2.0), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  EXPECT_DOUBLE_EQ(test_msg.float_value(), 2.0);
}

// Test that fields of type double are set correctly
TEST_P(CreateCreateStructStepTest, TestSetDoubleField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "double_value", CelValue::CreateDouble(2.0), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));
  EXPECT_DOUBLE_EQ(test_msg.double_value(), 2.0);
}

// Test that fields of type string are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetStringField) {
  const std::string kTestStr = "test";

  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "string_value", CelValue::CreateString(&kTestStr), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));
  EXPECT_EQ(test_msg.string_value(), kTestStr);
}


// Test that fields of type bytes are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetBytesField) {
  Arena arena;

  const std::string kTestStr = "test";
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bytes_value", CelValue::CreateBytes(&kTestStr), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));
  EXPECT_EQ(test_msg.bytes_value(), kTestStr);
}

// Test that fields of type duration are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetDurationField) {
  Arena arena;

  google::protobuf::Duration test_duration;
  test_duration.set_seconds(2);
  test_duration.set_nanos(3);
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "duration_value", CelProtoWrapper::CreateDuration(&test_duration), &arena,
      &test_msg, enable_unknowns(), enable_recursive_planning()));
  EXPECT_THAT(test_msg.duration_value(), EqualsProto(test_duration));
}

// Test that fields of type timestamp are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetTimestampField) {
  Arena arena;

  google::protobuf::Timestamp test_timestamp;
  test_timestamp.set_seconds(2);
  test_timestamp.set_nanos(3);
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "timestamp_value", CelProtoWrapper::CreateTimestamp(&test_timestamp),
      &arena, &test_msg, enable_unknowns(), enable_recursive_planning()));
  EXPECT_THAT(test_msg.timestamp_value(), EqualsProto(test_timestamp));
}

// Test that fields of type Message are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetMessageField) {
  Arena arena;

  // Create payload message and set some fields.
  TestMessage orig_msg;
  orig_msg.set_bool_value(true);
  orig_msg.set_string_value("test");

  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "message_value", CelProtoWrapper::CreateMessage(&orig_msg, &arena),
      &arena, &test_msg, enable_unknowns(), enable_recursive_planning()));
  EXPECT_THAT(test_msg.message_value(), EqualsProto(orig_msg));
}

// Test that fields of type Any are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetAnyField) {
  Arena arena;

  // Create payload message and set some fields.
  TestMessage orig_embedded_msg;
  orig_embedded_msg.set_bool_value(true);
  orig_embedded_msg.set_string_value("embedded");

  TestMessage orig_msg;
  orig_msg.mutable_any_value()->PackFrom(orig_embedded_msg);

  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "any_value", CelProtoWrapper::CreateMessage(&orig_embedded_msg, &arena),
      &arena, &test_msg, enable_unknowns(), enable_recursive_planning()));
  EXPECT_THAT(test_msg, EqualsProto(orig_msg));

  TestMessage test_embedded_msg;
  ASSERT_TRUE(test_msg.any_value().UnpackTo(&test_embedded_msg));
  EXPECT_THAT(test_embedded_msg, EqualsProto(orig_embedded_msg));
}

// Test that fields of type Message are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetEnumField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "enum_value", CelValue::CreateInt64(TestMessage::TEST_ENUM_2), &arena,
      &test_msg, enable_unknowns(), enable_recursive_planning()));
  EXPECT_EQ(test_msg.enum_value(), TestMessage::TEST_ENUM_2);
}

// Test that fields of type bool are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedBoolField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<bool> kValues = {true, false};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateBool(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bool_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.bool_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type int32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedInt32Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<int32_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateInt64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int32_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.int32_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type uint32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedUInt32Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<uint32_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateUint64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint32_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.uint32_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type int64_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedInt64Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<int64_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateInt64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int64_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.int64_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type uint64_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedUInt64Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<uint64_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateUint64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint64_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.uint64_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type float are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedFloatField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<float> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateDouble(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "float_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.float_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type uint32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedDoubleField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<double> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateDouble(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "double_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.double_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type String are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedStringField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::string> kValues = {"test1", "test2"};
  std::vector<CelValue> values;
  for (const auto& value : kValues) {
    values.push_back(CelValue::CreateString(&value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "string_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.string_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type String are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedBytesField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::string> kValues = {"test1", "test2"};
  std::vector<CelValue> values;
  for (const auto& value : kValues) {
    values.push_back(CelValue::CreateBytes(&value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bytes_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.bytes_list(), Pointwise(Eq(), kValues));
}


// Test that repeated fields of type Message are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedMessageField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<TestMessage> kValues(2);
  kValues[0].set_string_value("test1");
  kValues[1].set_string_value("test2");
  std::vector<CelValue> values;
  for (const auto& value : kValues) {
    values.push_back(CelProtoWrapper::CreateMessage(&value, &arena));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "message_list", values, &arena, &test_msg, enable_unknowns(),
      enable_recursive_planning()));
  ASSERT_THAT(test_msg.message_list()[0], EqualsProto(kValues[0]));
  ASSERT_THAT(test_msg.message_list()[1], EqualsProto(kValues[1]));
}


// Test that fields of type map<string, ...> are set correctly
TEST_P(CreateCreateStructStepTest, TestSetStringMapField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::pair<CelValue, CelValue>> entries;

  const std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back(
      {CelValue::CreateString(&kKeys[1]), CelValue::CreateInt64(1)});

  auto cel_map =
      *CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          entries.data(), entries.size()));

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "string_int32_map", CelValue::CreateMap(cel_map.get()), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  ASSERT_EQ(test_msg.string_int32_map().size(), 2);
  ASSERT_EQ(test_msg.string_int32_map().at(kKeys[0]), 2);
  ASSERT_EQ(test_msg.string_int32_map().at(kKeys[1]), 1);
}

// Test that fields of type map<int64_t, ...> are set correctly
TEST_P(CreateCreateStructStepTest, TestSetInt64MapField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::pair<CelValue, CelValue>> entries;

  const std::vector<int64_t> kKeys = {3, 4};

  entries.push_back(
      {CelValue::CreateInt64(kKeys[0]), CelValue::CreateInt64(1)});
  entries.push_back(
      {CelValue::CreateInt64(kKeys[1]), CelValue::CreateInt64(2)});

  auto cel_map =
      *CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          entries.data(), entries.size()));

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int64_int32_map", CelValue::CreateMap(cel_map.get()), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  ASSERT_EQ(test_msg.int64_int32_map().size(), 2);
  ASSERT_EQ(test_msg.int64_int32_map().at(kKeys[0]), 1);
  ASSERT_EQ(test_msg.int64_int32_map().at(kKeys[1]), 2);
}

// Test that fields of type map<uint64_t, ...> are set correctly
TEST_P(CreateCreateStructStepTest, TestSetUInt64MapField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::pair<CelValue, CelValue>> entries;

  const std::vector<uint64_t> kKeys = {3, 4};

  entries.push_back(
      {CelValue::CreateUint64(kKeys[0]), CelValue::CreateInt64(1)});
  entries.push_back(
      {CelValue::CreateUint64(kKeys[1]), CelValue::CreateInt64(2)});

  auto cel_map =
      *CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          entries.data(), entries.size()));

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint64_int32_map", CelValue::CreateMap(cel_map.get()), &arena, &test_msg,
      enable_unknowns(), enable_recursive_planning()));

  ASSERT_EQ(test_msg.uint64_int32_map().size(), 2);
  ASSERT_EQ(test_msg.uint64_int32_map().at(kKeys[0]), 1);
  ASSERT_EQ(test_msg.uint64_int32_map().at(kKeys[1]), 2);
}

INSTANTIATE_TEST_SUITE_P(CombinedCreateStructTest, CreateCreateStructStepTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace

}  // namespace google::api::expr::runtime
