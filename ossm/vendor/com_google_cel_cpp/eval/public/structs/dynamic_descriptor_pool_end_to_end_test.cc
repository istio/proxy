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

#include <cstdint>
#include <memory>

#include "cel/expr/syntax.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/structs/cel_proto_descriptor_pool_builder.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::protobuf::DescriptorPool;

constexpr int32_t kStartingFieldNumber = 512;
constexpr int32_t kIntFieldNumber = kStartingFieldNumber;
constexpr int32_t kStringFieldNumber = kStartingFieldNumber + 1;
constexpr int32_t kMessageFieldNumber = kStartingFieldNumber + 2;

MATCHER_P(CelEqualsProto, msg,
          absl::StrCat("CEL Equals ", msg->ShortDebugString())) {
  const google::protobuf::Message* got = arg;
  const google::protobuf::Message* want = msg;

  return google::protobuf::util::MessageDifferencer::Equals(*got, *want);
}

// Simulate a dynamic descriptor pool with an alternate definition for a linked
// type.
absl::Status AddTestTypes(DescriptorPool& pool) {
  google::protobuf::FileDescriptorProto file_descriptor;

  TestAllTypes::descriptor()->file()->CopyTo(&file_descriptor);
  auto* message_type_entry = file_descriptor.mutable_message_type(0);

  auto* dynamic_int_field = message_type_entry->add_field();
  dynamic_int_field->set_number(kIntFieldNumber);
  dynamic_int_field->set_name("dynamic_int_field");
  dynamic_int_field->set_type(google::protobuf::FieldDescriptorProto::TYPE_INT64);
  auto* dynamic_string_field = message_type_entry->add_field();
  dynamic_string_field->set_number(kStringFieldNumber);
  dynamic_string_field->set_name("dynamic_string_field");
  dynamic_string_field->set_type(google::protobuf::FieldDescriptorProto::TYPE_STRING);
  auto* dynamic_message_field = message_type_entry->add_field();
  dynamic_message_field->set_number(kMessageFieldNumber);
  dynamic_message_field->set_name("dynamic_message_field");
  dynamic_message_field->set_type(google::protobuf::FieldDescriptorProto::TYPE_MESSAGE);
  dynamic_message_field->set_type_name(
      ".cel.expr.conformance.proto3.TestAllTypes");

  CEL_RETURN_IF_ERROR(AddStandardMessageTypesToDescriptorPool(pool));
  if (!pool.BuildFile(file_descriptor)) {
    return absl::InternalError(
        "failed initializing custom descriptor pool for test.");
  }

  return absl::OkStatus();
}

class DynamicDescriptorPoolTest : public ::testing::Test {
 public:
  DynamicDescriptorPoolTest() : factory_(&descriptor_pool_) {}

  void SetUp() override { ASSERT_OK(AddTestTypes(descriptor_pool_)); }

 protected:
  absl::StatusOr<std::unique_ptr<google::protobuf::Message>> CreateMessageFromText(
      absl::string_view text_format) {
    const google::protobuf::Descriptor* dynamic_desc =
        descriptor_pool_.FindMessageTypeByName(
            "cel.expr.conformance.proto3.TestAllTypes");
    auto message = absl::WrapUnique(factory_.GetPrototype(dynamic_desc)->New());

    if (!google::protobuf::TextFormat::ParseFromString(text_format, message.get())) {
      return absl::InvalidArgumentError(
          "invalid text format for dynamic message");
    }

    return message;
  }

  DescriptorPool descriptor_pool_;
  google::protobuf::DynamicMessageFactory factory_;
  google::protobuf::Arena arena_;
};

TEST_F(DynamicDescriptorPoolTest, FieldAccess) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<google::protobuf::Message> message,
                       CreateMessageFromText("dynamic_int_field: 42"));
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("msg.dynamic_int_field < 50"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  CelValue val = CelProtoWrapper::CreateMessage(message.get(), &arena_);
  act.InsertValue("msg", val);
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST_F(DynamicDescriptorPoolTest, Create) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  builder->set_container("cel.expr.conformance.proto3");

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse(
                                            R"cel(
      TestAllTypes{
        dynamic_int_field: 42,
        dynamic_string_field: "string",
        dynamic_message_field: TestAllTypes{dynamic_int_field: 50 }
      }
    )cel"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  ASSERT_OK_AND_ASSIGN(auto expected, CreateMessageFromText(R"pb(
                         dynamic_int_field: 42
                         dynamic_string_field: "string"
                         dynamic_message_field { dynamic_int_field: 50 }
                       )pb"));

  EXPECT_THAT(result, test::IsCelMessage(CelEqualsProto(expected.get())));
}

TEST_F(DynamicDescriptorPoolTest, AnyUnpack) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(
      auto message, CreateMessageFromText(R"pb(
        single_any {
          [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
            dynamic_int_field: 45
          }
        }
      )pb"));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("msg.single_any.dynamic_int_field < 50"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  CelValue val = CelProtoWrapper::CreateMessage(message.get(), &arena_);
  act.InsertValue("msg", val);
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST_F(DynamicDescriptorPoolTest, AnyWrapperUnpack) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(
      auto message, CreateMessageFromText(R"pb(
        single_any {
          [type.googleapis.com/google.protobuf.Int64Value] { value: 45 }
        }
      )pb"));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("msg.single_any < 50"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  CelValue val = CelProtoWrapper::CreateMessage(message.get(), &arena_);
  act.InsertValue("msg", val);
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST_F(DynamicDescriptorPoolTest, AnyUnpackRepeated) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(
      auto message, CreateMessageFromText(R"pb(
        repeated_any {
          [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
            dynamic_int_field: 0
          }
        }
        repeated_any {
          [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
            dynamic_int_field: 1
          }
        }
      )pb"));

  ASSERT_OK_AND_ASSIGN(
      ParsedExpr expr,
      Parse("msg.repeated_any.exists(x, x.dynamic_int_field > 2)"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  CelValue val = CelProtoWrapper::CreateMessage(message.get(), &arena_);
  act.InsertValue("msg", val);
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  EXPECT_THAT(result, test::IsCelBool(false));
}

TEST_F(DynamicDescriptorPoolTest, AnyPack) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  builder->set_container("cel.expr.conformance.proto3");

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse(R"cel(
                        TestAllTypes{
                          single_any: TestAllTypes{dynamic_int_field: 42}
                        })cel"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  ASSERT_OK_AND_ASSIGN(
      auto expected_message, CreateMessageFromText(R"pb(
        single_any {
          [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
            dynamic_int_field: 42
          }
        }
      )pb"));
  EXPECT_THAT(result,
              test::IsCelMessage(CelEqualsProto(expected_message.get())));
}

TEST_F(DynamicDescriptorPoolTest, AnyWrapperPack) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  builder->set_container("cel.expr.conformance.proto3");

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse(R"cel(
                        TestAllTypes{
                          single_any: 42
                        })cel"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  ASSERT_OK_AND_ASSIGN(
      auto expected_message, CreateMessageFromText(R"pb(
        single_any {
          [type.googleapis.com/google.protobuf.Int64Value] { value: 42 }
        }
      )pb"));
  EXPECT_THAT(result,
              test::IsCelMessage(CelEqualsProto(expected_message.get())));
}

TEST_F(DynamicDescriptorPoolTest, AnyPackRepeated) {
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(&descriptor_pool_, &factory_, options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  builder->set_container("cel.expr.conformance.proto3");

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse(R"cel(
                        TestAllTypes{
                          repeated_any: [
                            TestAllTypes{dynamic_int_field: 0},
                            TestAllTypes{dynamic_int_field: 1},
                          ]
                        })cel"));
  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation act;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(act, &arena_));

  ASSERT_OK_AND_ASSIGN(
      auto expected_message, CreateMessageFromText(R"pb(
        repeated_any {
          [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
            dynamic_int_field: 0
          }
        }
        repeated_any {
          [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
            dynamic_int_field: 1
          }
        }
      )pb"));
  EXPECT_THAT(result,
              test::IsCelMessage(CelEqualsProto(expected_message.get())));
}

}  // namespace
}  // namespace google::api::expr::runtime
