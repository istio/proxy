#include "eval/eval/select_step.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/attribute.h"
#include "base/attribute_set.h"
#include "base/type_provider.h"
#include "common/casting.h"
#include "common/expr.h"
#include "common/legacy_value.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_extensions.pb.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/value.h"
#include "internal/proto_matchers.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "runtime/activation.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/internal/runtime_type_provider.h"
#include "runtime/runtime_options.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::Attribute;
using ::cel::AttributeQualifier;
using ::cel::AttributeSet;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::Expr;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::OptionalValue;
using ::cel::RuntimeOptions;
using ::cel::TypeProvider;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::extensions::ProtoMessageToValue;
using ::cel::internal::test::EqualsProto;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::runtime_internal::RuntimeEnv;
using ::cel::test::IntValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

struct RunExpressionOptions {
  bool enable_unknowns = false;
  bool enable_wrapper_type_null_unboxing = false;
};

// Simple implementation LegacyTypeAccessApis / LegacyTypeInfoApis that allows
// mocking for getters/setters.
class MockAccessor : public LegacyTypeAccessApis, public LegacyTypeInfoApis {
 public:
  MOCK_METHOD(absl::StatusOr<bool>, HasField,
              (absl::string_view field_name,
               const CelValue::MessageWrapper& value),
              (const, override));
  MOCK_METHOD(absl::StatusOr<CelValue>, GetField,
              (absl::string_view field_name,
               const CelValue::MessageWrapper& instance,
               ProtoWrapperTypeOptions unboxing_option,
               cel::MemoryManagerRef memory_manager),
              (const, override));
  MOCK_METHOD(absl::string_view, GetTypename,
              (const CelValue::MessageWrapper& instance), (const, override));
  MOCK_METHOD(std::string, DebugString,
              (const CelValue::MessageWrapper& instance), (const, override));
  MOCK_METHOD(std::vector<absl::string_view>, ListFields,
              (const CelValue::MessageWrapper& value), (const, override));
  const LegacyTypeAccessApis* GetAccessApis(
      const CelValue::MessageWrapper& instance) const override {
    return this;
  }
};

class SelectStepTest : public testing::Test {
 public:
  SelectStepTest() : env_(NewTestingRuntimeEnv()) {}
  // Helper method. Creates simple pipeline containing Select step and runs it.
  absl::StatusOr<CelValue> RunExpression(const CelValue target,
                                         absl::string_view field, bool test,
                                         absl::string_view unknown_path,
                                         RunExpressionOptions options) {
    ExecutionPath path;

    Expr expr;
    auto& select = expr.mutable_select_expr();
    select.set_field(std::string(field));
    select.set_test_only(test);
    Expr& expr0 = select.mutable_operand();

    auto& ident = expr0.mutable_ident_expr();
    ident.set_name("target");
    CEL_ASSIGN_OR_RETURN(auto step0, CreateIdentStep(ident, expr0.id()));
    CEL_ASSIGN_OR_RETURN(
        auto step1,
        CreateSelectStep(select, expr.id(),
                         options.enable_wrapper_type_null_unboxing));

    path.push_back(std::move(step0));
    path.push_back(std::move(step1));

    cel::RuntimeOptions runtime_options;
    if (options.enable_unknowns) {
      runtime_options.unknown_processing =
          cel::UnknownProcessingOptions::kAttributeOnly;
    }
    CelExpressionFlatImpl cel_expr(
        env_, FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                             env_->type_registry.GetComposedTypeProvider(),
                             runtime_options));
    Activation activation;
    activation.InsertValue("target", target);

    return cel_expr.Evaluate(activation, &arena_);
  }

  absl::StatusOr<CelValue> RunExpression(const TestExtensions* message,
                                         absl::string_view field, bool test,
                                         RunExpressionOptions options) {
    return RunExpression(CelProtoWrapper::CreateMessage(message, &arena_),
                         field, test, "", options);
  }

  absl::StatusOr<CelValue> RunExpression(const TestMessage* message,
                                         absl::string_view field, bool test,
                                         absl::string_view unknown_path,
                                         RunExpressionOptions options) {
    return RunExpression(CelProtoWrapper::CreateMessage(message, &arena_),
                         field, test, unknown_path, options);
  }

  absl::StatusOr<CelValue> RunExpression(const TestMessage* message,
                                         absl::string_view field, bool test,
                                         RunExpressionOptions options) {
    return RunExpression(message, field, test, "", options);
  }

  absl::StatusOr<CelValue> RunExpression(const CelMap* map_value,
                                         absl::string_view field, bool test,
                                         absl::string_view unknown_path,
                                         RunExpressionOptions options) {
    return RunExpression(CelValue::CreateMap(map_value), field, test,
                         unknown_path, options);
  }

  absl::StatusOr<CelValue> RunExpression(const CelMap* map_value,
                                         absl::string_view field, bool test,
                                         RunExpressionOptions options) {
    return RunExpression(map_value, field, test, "", options);
  }

 protected:
  absl_nonnull std::shared_ptr<const RuntimeEnv> env_;
  google::protobuf::Arena arena_;
};

class SelectStepConformanceTest : public SelectStepTest,
                                  public testing::WithParamInterface<bool> {};

TEST_P(SelectStepConformanceTest, SelectMessageIsNull) {
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(static_cast<const TestMessage*>(nullptr),
                                     "bool_value", true, options));

  ASSERT_TRUE(result.IsError());
}

TEST_P(SelectStepConformanceTest, SelectTargetNotStructOrMap) {
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(CelValue::CreateStringView("some_value"), "some_field",
                    /*test=*/false,
                    /*unknown_path=*/"", options));

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Applying SELECT to non-message type")));
}

TEST_P(SelectStepConformanceTest, PresenseIsFalseTest) {
  TestMessage message;
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "bool_value", true, options));

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST_P(SelectStepConformanceTest, PresenseIsTrueTest) {
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  TestMessage message;
  message.set_bool_value(true);

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "bool_value", true, options));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_P(SelectStepConformanceTest, ExtensionsPresenceIsTrueTest) {
  TestExtensions exts;
  TestExtensions* nested = exts.MutableExtension(nested_ext);
  nested->set_name("nested");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&exts, "google.api.expr.runtime.nested_ext", true,
                    options));

  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST_P(SelectStepConformanceTest, ExtensionsPresenceIsFalseTest) {
  TestExtensions exts;
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&exts, "google.api.expr.runtime.nested_ext", true,
                    options));

  ASSERT_TRUE(result.IsBool());
  EXPECT_FALSE(result.BoolOrDie());
}

TEST_P(SelectStepConformanceTest, MapPresenseIsFalseTest) {
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)}};

  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(map_value.get(), "key2", true, options));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST_P(SelectStepConformanceTest, MapPresenseIsTrueTest) {
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)}};

  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(map_value.get(), "key1", true, options));

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_F(SelectStepTest, MapPresenseIsErrorTest) {
  TestMessage message;

  Expr select_expr;
  auto& select = select_expr.mutable_select_expr();
  select.set_field("1");
  select.set_test_only(true);
  Expr& expr1 = select.mutable_operand();
  auto& select_map = expr1.mutable_select_expr();
  select_map.set_field("int32_int32_map");
  Expr& expr0 = select_map.mutable_operand();
  auto& ident = expr0.mutable_ident_expr();
  ident.set_name("target");

  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0.id()));
  ASSERT_OK_AND_ASSIGN(
      auto step1,
      CreateSelectStep(select_map, expr1.id(),
                       /*enable_wrapper_type_null_unboxing=*/false));
  ASSERT_OK_AND_ASSIGN(
      auto step2,
      CreateSelectStep(select, select_expr.id(),
                       /*enable_wrapper_type_null_unboxing=*/false));

  ExecutionPath path;
  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  CelExpressionFlatImpl cel_expr(
      env_, FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                           env_->type_registry.GetComposedTypeProvider(),
                           cel::RuntimeOptions{}));
  Activation activation;
  activation.InsertValue("target",
                         CelProtoWrapper::CreateMessage(&message, &arena_));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena_));
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.ErrorOrDie()->code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(SelectStepTest, MapPresenseIsTrueWithUnknownTest) {
  UnknownSet unknown_set;
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1),
       CelValue::CreateUnknownSet(&unknown_set)}};

  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();

  RunExpressionOptions options;
  options.enable_unknowns = true;

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(map_value.get(), "key1", true, options));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_P(SelectStepConformanceTest, FieldIsNotPresentInProtoTest) {
  TestMessage message;

  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "fake_field", false, options));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(result.ErrorOrDie()->code(), Eq(absl::StatusCode::kNotFound));
}

TEST_P(SelectStepConformanceTest, FieldIsNotSetTest) {
  TestMessage message;
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "bool_value", false, options));

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST_P(SelectStepConformanceTest, SimpleBoolTest) {
  TestMessage message;
  message.set_bool_value(true);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "bool_value", false, options));

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_P(SelectStepConformanceTest, SimpleInt32Test) {
  TestMessage message;
  message.set_int32_value(1);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "int32_value", false, options));

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

TEST_P(SelectStepConformanceTest, SimpleInt64Test) {
  TestMessage message;
  message.set_int64_value(1);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "int64_value", false, options));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

TEST_P(SelectStepConformanceTest, SimpleUInt32Test) {
  TestMessage message;
  message.set_uint32_value(1);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "uint32_value", false, options));

  ASSERT_TRUE(result.IsUint64());
  EXPECT_EQ(result.Uint64OrDie(), 1);
}

TEST_P(SelectStepConformanceTest, SimpleUint64Test) {
  TestMessage message;
  message.set_uint64_value(1);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "uint64_value", false, options));

  ASSERT_TRUE(result.IsUint64());
  EXPECT_EQ(result.Uint64OrDie(), 1);
}

TEST_P(SelectStepConformanceTest, SimpleStringTest) {
  TestMessage message;
  std::string value = "test";
  message.set_string_value(value);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "string_value", false, options));

  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.StringOrDie().value(), "test");
}

TEST_P(SelectStepConformanceTest, WrapperTypeNullUnboxingEnabledTest) {
  TestMessage message;
  message.mutable_string_wrapper_value()->set_value("test");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  options.enable_wrapper_type_null_unboxing = true;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "string_wrapper_value", false, options));

  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.StringOrDie().value(), "test");
  ASSERT_OK_AND_ASSIGN(
      result, RunExpression(&message, "int32_wrapper_value", false, options));
  EXPECT_TRUE(result.IsNull());
}

TEST_P(SelectStepConformanceTest, WrapperTypeNullUnboxingDisabledTest) {
  TestMessage message;
  message.mutable_string_wrapper_value()->set_value("test");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  options.enable_wrapper_type_null_unboxing = false;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "string_wrapper_value", false, options));

  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.StringOrDie().value(), "test");
  ASSERT_OK_AND_ASSIGN(
      result, RunExpression(&message, "int32_wrapper_value", false, options));
  EXPECT_TRUE(result.IsInt64());
}

TEST_P(SelectStepConformanceTest, SimpleBytesTest) {
  TestMessage message;
  std::string value = "test";
  message.set_bytes_value(value);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "bytes_value", false, options));

  ASSERT_TRUE(result.IsBytes());
  EXPECT_EQ(result.BytesOrDie().value(), "test");
}

TEST_P(SelectStepConformanceTest, SimpleMessageTest) {
  TestMessage message;
  TestMessage* message2 = message.mutable_message_value();
  message2->set_int32_value(1);
  message2->set_string_value("test");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result, RunExpression(&message, "message_value",
                                                      false, options));

  ASSERT_TRUE(result.IsMessage());
  EXPECT_THAT(*message2, EqualsProto(*result.MessageOrDie()));
}

TEST_P(SelectStepConformanceTest, GlobalExtensionsIntTest) {
  TestExtensions exts;
  exts.SetExtension(int32_ext, 42);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&exts, "google.api.expr.runtime.int32_ext",
                                     false, options));

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 42L);
}

TEST_P(SelectStepConformanceTest, GlobalExtensionsMessageTest) {
  TestExtensions exts;
  TestExtensions* nested = exts.MutableExtension(nested_ext);
  nested->set_name("nested");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&exts, "google.api.expr.runtime.nested_ext", false,
                    options));

  ASSERT_TRUE(result.IsMessage());
  EXPECT_THAT(result.MessageOrDie(), Eq(nested));
}

TEST_P(SelectStepConformanceTest, GlobalExtensionsMessageUnsetTest) {
  TestExtensions exts;
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&exts, "google.api.expr.runtime.nested_ext", false,
                    options));

  ASSERT_TRUE(result.IsMessage());
  EXPECT_THAT(result.MessageOrDie(), Eq(&TestExtensions::default_instance()));
}

TEST_P(SelectStepConformanceTest, GlobalExtensionsWrapperTest) {
  TestExtensions exts;
  google::protobuf::Int32Value* wrapper =
      exts.MutableExtension(int32_wrapper_ext);
  wrapper->set_value(42);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&exts, "google.api.expr.runtime.int32_wrapper_ext", false,
                    options));

  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(42L));
}

TEST_P(SelectStepConformanceTest, GlobalExtensionsWrapperUnsetTest) {
  TestExtensions exts;
  RunExpressionOptions options;
  options.enable_wrapper_type_null_unboxing = true;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&exts, "google.api.expr.runtime.int32_wrapper_ext", false,
                    options));

  ASSERT_TRUE(result.IsNull());
}

TEST_P(SelectStepConformanceTest, MessageExtensionsEnumTest) {
  TestExtensions exts;
  exts.SetExtension(TestMessageExtensions::enum_ext, TestExtEnum::TEST_EXT_1);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&exts,
                    "google.api.expr.runtime.TestMessageExtensions.enum_ext",
                    false, options));

  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestExtEnum::TEST_EXT_1));
}

TEST_P(SelectStepConformanceTest, MessageExtensionsRepeatedStringTest) {
  TestExtensions exts;
  exts.AddExtension(TestMessageExtensions::repeated_string_exts, "test1");
  exts.AddExtension(TestMessageExtensions::repeated_string_exts, "test2");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(
          &exts,
          "google.api.expr.runtime.TestMessageExtensions.repeated_string_exts",
          false, options));

  ASSERT_TRUE(result.IsList());
  const CelList* cel_list = result.ListOrDie();
  EXPECT_THAT(cel_list->size(), Eq(2));
}

TEST_P(SelectStepConformanceTest, MessageExtensionsRepeatedStringUnsetTest) {
  TestExtensions exts;
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(
          &exts,
          "google.api.expr.runtime.TestMessageExtensions.repeated_string_exts",
          false, options));

  ASSERT_TRUE(result.IsList());
  const CelList* cel_list = result.ListOrDie();
  EXPECT_THAT(cel_list->size(), Eq(0));
}

TEST_P(SelectStepConformanceTest, NullMessageAccessor) {
  TestMessage message;
  TestMessage* message2 = message.mutable_message_value();
  message2->set_int32_value(1);
  message2->set_string_value("test");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  CelValue value = CelValue::CreateMessageWrapper(
      CelValue::MessageWrapper(&message, TrivialTypeInfo::GetInstance()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(value, "message_value",
                                     /*test=*/false,
                                     /*unknown_path=*/"", options));

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(), StatusIs(absl::StatusCode::kNotFound));

  // same for has
  ASSERT_OK_AND_ASSIGN(result, RunExpression(value, "message_value",
                                             /*test=*/true,
                                             /*unknown_path=*/"", options));

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(), StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(SelectStepConformanceTest, CustomAccessor) {
  TestMessage message;
  TestMessage* message2 = message.mutable_message_value();
  message2->set_int32_value(1);
  message2->set_string_value("test");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  testing::NiceMock<MockAccessor> accessor;
  CelValue value = CelValue::CreateMessageWrapper(
      CelValue::MessageWrapper(&message, &accessor));

  ON_CALL(accessor, GetField(_, _, _, _))
      .WillByDefault(Return(CelValue::CreateInt64(2)));
  ON_CALL(accessor, HasField(_, _)).WillByDefault(Return(false));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(value, "message_value",
                                     /*test=*/false,
                                     /*unknown_path=*/"", options));

  EXPECT_THAT(result, test::IsCelInt64(2));

  // testonly select (has)
  ASSERT_OK_AND_ASSIGN(result, RunExpression(value, "message_value",
                                             /*test=*/true,
                                             /*unknown_path=*/"", options));

  EXPECT_THAT(result, test::IsCelBool(false));
}

TEST_P(SelectStepConformanceTest, CustomAccessorErrorHandling) {
  TestMessage message;
  TestMessage* message2 = message.mutable_message_value();
  message2->set_int32_value(1);
  message2->set_string_value("test");
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();
  testing::NiceMock<MockAccessor> accessor;
  CelValue value = CelValue::CreateMessageWrapper(
      CelValue::MessageWrapper(&message, &accessor));

  ON_CALL(accessor, GetField(_, _, _, _))
      .WillByDefault(Return(absl::InternalError("bad data")));
  ON_CALL(accessor, HasField(_, _))
      .WillByDefault(Return(absl::NotFoundError("not found")));

  // For get field, implementation may return an error-type cel value or a
  // status (e.g. broken assumption using a core type).
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(value, "message_value",
                                     /*test=*/false,
                                     /*unknown_path=*/"", options));
  EXPECT_THAT(result, test::IsCelError(StatusIs(absl::StatusCode::kInternal)));

  // testonly select (has) errors are coerced to CelError.
  ASSERT_OK_AND_ASSIGN(result, RunExpression(value, "message_value",
                                             /*test=*/true,
                                             /*unknown_path=*/"", options));

  EXPECT_THAT(result, test::IsCelError(StatusIs(absl::StatusCode::kNotFound)));
}

TEST_P(SelectStepConformanceTest, SimpleEnumTest) {
  TestMessage message;
  message.set_enum_value(TestMessage::TEST_ENUM_1);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "enum_value", false, options));

  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST_P(SelectStepConformanceTest, SimpleListTest) {
  TestMessage message;
  message.add_int32_list(1);
  message.add_int32_list(2);
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(&message, "int32_list", false, options));

  ASSERT_TRUE(result.IsList());
  const CelList* cel_list = result.ListOrDie();
  EXPECT_THAT(cel_list->size(), Eq(2));
}

TEST_P(SelectStepConformanceTest, SimpleMapTest) {
  TestMessage message;
  auto map_field = message.mutable_string_int32_map();
  (*map_field)["test0"] = 1;
  (*map_field)["test1"] = 2;
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "string_int32_map", false, options));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  EXPECT_THAT(cel_map->size(), Eq(2));
}

TEST_P(SelectStepConformanceTest, MapSimpleInt32Test) {
  std::string key1 = "key1";
  std::string key2 = "key2";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)},
      {CelValue::CreateString(&key2), CelValue::CreateInt64(2)}};
  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();
  RunExpressionOptions options;
  options.enable_unknowns = GetParam();

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(map_value.get(), "key1", false, options));

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

// Test Select behavior, when expression to select from is an Error.
TEST_P(SelectStepConformanceTest, CelErrorAsArgument) {
  ExecutionPath path;

  Expr dummy_expr;

  auto& select = dummy_expr.mutable_select_expr();
  select.set_field("position");
  select.set_test_only(false);
  Expr& expr0 = select.mutable_operand();

  auto& ident = expr0.mutable_ident_expr();
  ident.set_name("message");
  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0.id()));
  ASSERT_OK_AND_ASSIGN(
      auto step1,
      CreateSelectStep(select, dummy_expr.id(),
                       /*enable_wrapper_type_null_unboxing=*/false));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelError error = absl::CancelledError();

  cel::RuntimeOptions options;
  if (GetParam()) {
    options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  }
  CelExpressionFlatImpl cel_expr(
      env_,
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     env_->type_registry.GetComposedTypeProvider(), options));
  Activation activation;
  activation.InsertValue("message", CelValue::CreateError(&error));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena_));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(), Eq(error));
}

TEST_F(SelectStepTest, DisableMissingAttributeOK) {
  TestMessage message;
  message.set_bool_value(true);
  ExecutionPath path;

  Expr dummy_expr;

  auto& select = dummy_expr.mutable_select_expr();
  select.set_field("bool_value");
  select.set_test_only(false);
  Expr& expr0 = select.mutable_operand();

  auto& ident = expr0.mutable_ident_expr();
  ident.set_name("message");
  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0.id()));
  ASSERT_OK_AND_ASSIGN(
      auto step1,
      CreateSelectStep(select, dummy_expr.id(),
                       /*enable_wrapper_type_null_unboxing=*/false));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelExpressionFlatImpl cel_expr(
      env_, FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                           env_->type_registry.GetComposedTypeProvider(),
                           cel::RuntimeOptions{}));
  Activation activation;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena_));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena_));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  CelAttributePattern pattern("message", {});
  activation.set_missing_attribute_patterns({pattern});

  ASSERT_OK_AND_ASSIGN(result, cel_expr.Evaluate(activation, &arena_));
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_F(SelectStepTest, UnrecoverableUnknownValueProducesError) {
  TestMessage message;
  message.set_bool_value(true);
  ExecutionPath path;

  Expr dummy_expr;

  auto& select = dummy_expr.mutable_select_expr();
  select.set_field("bool_value");
  select.set_test_only(false);
  Expr& expr0 = select.mutable_operand();

  auto& ident = expr0.mutable_ident_expr();
  ident.set_name("message");
  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0.id()));
  ASSERT_OK_AND_ASSIGN(
      auto step1,
      CreateSelectStep(select, dummy_expr.id(),
                       /*enable_wrapper_type_null_unboxing=*/false));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  cel::RuntimeOptions options;
  options.enable_missing_attribute_errors = true;
  CelExpressionFlatImpl cel_expr(
      env_,
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     env_->type_registry.GetComposedTypeProvider(), options));
  Activation activation;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena_));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena_));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  CelAttributePattern pattern("message",
                              {CreateCelAttributeQualifierPattern(
                                  CelValue::CreateStringView("bool_value"))});
  activation.set_missing_attribute_patterns({pattern});

  ASSERT_OK_AND_ASSIGN(result, cel_expr.Evaluate(activation, &arena_));
  EXPECT_THAT(*result.ErrorOrDie(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("MissingAttributeError: message.bool_value")));
}

TEST_F(SelectStepTest, UnknownPatternResolvesToUnknown) {
  TestMessage message;
  message.set_bool_value(true);
  ExecutionPath path;

  Expr dummy_expr;

  auto& select = dummy_expr.mutable_select_expr();
  select.set_field("bool_value");
  select.set_test_only(false);
  Expr& expr0 = select.mutable_operand();

  auto& ident = expr0.mutable_ident_expr();
  ident.set_name("message");
  auto step0_status = CreateIdentStep(ident, expr0.id());
  auto step1_status =
      CreateSelectStep(select, dummy_expr.id(),
                       /*enable_wrapper_type_null_unboxing=*/false);

  ASSERT_THAT(step0_status, IsOk());
  ASSERT_THAT(step1_status, IsOk());

  path.push_back(*std::move(step0_status));
  path.push_back(*std::move(step1_status));

  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  CelExpressionFlatImpl cel_expr(
      env_,
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     env_->type_registry.GetComposedTypeProvider(), options));

  {
    std::vector<CelAttributePattern> unknown_patterns;
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena_));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena_));
    ASSERT_TRUE(result.IsBool());
    EXPECT_EQ(result.BoolOrDie(), true);
  }

  const std::string kSegmentCorrect1 = "bool_value";
  const std::string kSegmentIncorrect = "message_value";

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern("message", {}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena_));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena_));
    ASSERT_TRUE(result.IsUnknownSet());
  }

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern(
        "message", {CreateCelAttributeQualifierPattern(
                       CelValue::CreateString(&kSegmentCorrect1))}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena_));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena_));
    ASSERT_TRUE(result.IsUnknownSet());
  }

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern(
        "message", {CelAttributeQualifierPattern::CreateWildcard()}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena_));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena_));
    ASSERT_TRUE(result.IsUnknownSet());
  }

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern(
        "message", {CreateCelAttributeQualifierPattern(
                       CelValue::CreateString(&kSegmentIncorrect))}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena_));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena_));
    ASSERT_TRUE(result.IsBool());
    EXPECT_EQ(result.BoolOrDie(), true);
  }
}

INSTANTIATE_TEST_SUITE_P(UnknownsEnabled, SelectStepConformanceTest,
                         testing::Bool());

class DirectSelectStepTest : public testing::Test {
 public:
  DirectSelectStepTest()
      : type_provider_(cel::internal::GetTestingDescriptorPool()) {}

  cel::Value TestWrapMessage(const google::protobuf::Message* message) {
    CelValue value = CelProtoWrapper::CreateMessage(message, &arena_);
    auto result = cel::interop_internal::FromLegacyValue(&arena_, value);
    ABSL_DCHECK_OK(result.status());
    return std::move(result).value();
  }

  std::vector<std::string> AttributeStrings(const UnknownValue& v) {
    std::vector<std::string> result;
    for (const Attribute& attr : v.attribute_set()) {
      auto attr_str = attr.AsString();
      ABSL_DCHECK_OK(attr_str.status());
      result.push_back(std::move(attr_str).value());
    }
    return result;
  }

 protected:
  google::protobuf::Arena arena_;
  cel::runtime_internal::RuntimeTypeProvider type_provider_;
};

TEST_F(DirectSelectStepTest, SelectFromMap) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(
      CreateDirectIdentStep("map_val", -1), cel::StringValue("one"),
      /*test_only=*/false, -1,
      /*enable_wrapper_type_null_unboxing=*/true);

  auto map_builder = cel::NewMapValueBuilder(&arena_);
  ASSERT_THAT(map_builder->Put(cel::StringValue("one"), IntValue(1)), IsOk());
  ASSERT_THAT(map_builder->Put(cel::StringValue("two"), IntValue(2)), IsOk());
  activation.InsertOrAssignValue("map_val", std::move(*map_builder).Build());

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<IntValue>(result));

  EXPECT_EQ(Cast<IntValue>(result).NativeValue(), 1);
}

TEST_F(DirectSelectStepTest, HasMap) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(
      CreateDirectIdentStep("map_val", -1), cel::StringValue("two"),
      /*test_only=*/true, -1,
      /*enable_wrapper_type_null_unboxing=*/true);

  auto map_builder = cel::NewMapValueBuilder(&arena_);
  ASSERT_THAT(map_builder->Put(cel::StringValue("one"), IntValue(1)), IsOk());
  ASSERT_THAT(map_builder->Put(cel::StringValue("two"), IntValue(2)), IsOk());
  activation.InsertOrAssignValue("map_val", std::move(*map_builder).Build());

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<BoolValue>(result));

  EXPECT_TRUE(Cast<BoolValue>(result).NativeValue());
}

TEST_F(DirectSelectStepTest, SelectFromOptionalMap) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(CreateDirectIdentStep("map_val", -1),
                                     cel::StringValue("one"),
                                     /*test_only=*/false, -1,
                                     /*enable_wrapper_type_null_unboxing=*/true,
                                     /*enable_optional_types=*/true);

  auto map_builder = cel::NewMapValueBuilder(&arena_);
  ASSERT_THAT(map_builder->Put(cel::StringValue("one"), IntValue(1)), IsOk());
  ASSERT_THAT(map_builder->Put(cel::StringValue("two"), IntValue(2)), IsOk());
  activation.InsertOrAssignValue(
      "map_val", OptionalValue::Of(std::move(*map_builder).Build(), &arena_));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<OptionalValue>(result));
  EXPECT_THAT(Cast<OptionalValue>(static_cast<const Value&>(result)).Value(),
              IntValueIs(1));
}

TEST_F(DirectSelectStepTest, SelectFromOptionalMapAbsent) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(CreateDirectIdentStep("map_val", -1),
                                     cel::StringValue("three"),
                                     /*test_only=*/false, -1,
                                     /*enable_wrapper_type_null_unboxing=*/true,
                                     /*enable_optional_types=*/true);

  auto map_builder = cel::NewMapValueBuilder(&arena_);
  ASSERT_THAT(map_builder->Put(cel::StringValue("one"), IntValue(1)), IsOk());
  ASSERT_THAT(map_builder->Put(cel::StringValue("two"), IntValue(2)), IsOk());
  activation.InsertOrAssignValue(
      "map_val", OptionalValue::Of(std::move(*map_builder).Build(), &arena_));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<OptionalValue>(result));
  EXPECT_FALSE(
      Cast<OptionalValue>(static_cast<const Value&>(result)).HasValue());
}

TEST_F(DirectSelectStepTest, SelectFromOptionalStruct) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(CreateDirectIdentStep("struct_val", -1),
                                     cel::StringValue("single_int64"),
                                     /*test_only=*/false, -1,
                                     /*enable_wrapper_type_null_unboxing=*/true,
                                     /*enable_optional_types=*/true);

  TestAllTypes message;
  message.set_single_int64(1);

  ASSERT_OK_AND_ASSIGN(
      Value struct_val,
      ProtoMessageToValue(std::move(message),
                          cel::internal::GetTestingDescriptorPool(),
                          cel::internal::GetTestingMessageFactory(), &arena_));

  activation.InsertOrAssignValue("struct_val",
                                 OptionalValue::Of(struct_val, &arena_));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<OptionalValue>(result));
  EXPECT_THAT(Cast<OptionalValue>(static_cast<const Value&>(result)).Value(),
              IntValueIs(1));
}

TEST_F(DirectSelectStepTest, SelectFromOptionalStructFieldNotSet) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(CreateDirectIdentStep("struct_val", -1),
                                     cel::StringValue("single_string"),
                                     /*test_only=*/false, -1,
                                     /*enable_wrapper_type_null_unboxing=*/true,
                                     /*enable_optional_types=*/true);

  TestAllTypes message;
  message.set_single_int64(1);

  ASSERT_OK_AND_ASSIGN(
      Value struct_val,
      ProtoMessageToValue(std::move(message),
                          cel::internal::GetTestingDescriptorPool(),
                          cel::internal::GetTestingMessageFactory(), &arena_));

  activation.InsertOrAssignValue("struct_val",
                                 OptionalValue::Of(struct_val, &arena_));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<OptionalValue>(result));
  EXPECT_FALSE(
      Cast<OptionalValue>(static_cast<const Value&>(result)).HasValue());
}

TEST_F(DirectSelectStepTest, SelectFromEmptyOptional) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(CreateDirectIdentStep("map_val", -1),
                                     cel::StringValue("one"),
                                     /*test_only=*/false, -1,
                                     /*enable_wrapper_type_null_unboxing=*/true,
                                     /*enable_optional_types=*/true);

  activation.InsertOrAssignValue("map_val", OptionalValue::None());

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<OptionalValue>(result));
  EXPECT_FALSE(
      cel::Cast<OptionalValue>(static_cast<const Value&>(result)).HasValue());
}

TEST_F(DirectSelectStepTest, HasOptional) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(CreateDirectIdentStep("map_val", -1),
                                     cel::StringValue("two"),
                                     /*test_only=*/true, -1,
                                     /*enable_wrapper_type_null_unboxing=*/true,
                                     /*enable_optional_types=*/true);

  auto map_builder = cel::NewMapValueBuilder(&arena_);
  ASSERT_THAT(map_builder->Put(cel::StringValue("one"), IntValue(1)), IsOk());
  ASSERT_THAT(map_builder->Put(cel::StringValue("two"), IntValue(2)), IsOk());
  activation.InsertOrAssignValue(
      "map_val", OptionalValue::Of(std::move(*map_builder).Build(), &arena_));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<BoolValue>(result));

  EXPECT_TRUE(Cast<BoolValue>(result).NativeValue());
}

TEST_F(DirectSelectStepTest, HasEmptyOptional) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(CreateDirectIdentStep("map_val", -1),
                                     cel::StringValue("two"),
                                     /*test_only=*/true, -1,
                                     /*enable_wrapper_type_null_unboxing=*/true,
                                     /*enable_optional_types=*/true);

  activation.InsertOrAssignValue("map_val", OptionalValue::None());

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<BoolValue>(result));

  EXPECT_FALSE(Cast<BoolValue>(result).NativeValue());
}

TEST_F(DirectSelectStepTest, SelectFromStruct) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step =
      CreateDirectSelectStep(CreateDirectIdentStep("test_all_types", -1),
                             cel::StringValue("single_int64"),
                             /*test_only=*/false, -1,
                             /*enable_wrapper_type_null_unboxing=*/true);

  TestAllTypes message;
  message.set_single_int64(1);
  activation.InsertOrAssignValue("test_all_types", TestWrapMessage(&message));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<IntValue>(result));

  EXPECT_EQ(Cast<IntValue>(result).NativeValue(), 1);
}

TEST_F(DirectSelectStepTest, HasStruct) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step =
      CreateDirectSelectStep(CreateDirectIdentStep("test_all_types", -1),
                             cel::StringValue("single_string"),
                             /*test_only=*/true, -1,
                             /*enable_wrapper_type_null_unboxing=*/true);

  TestAllTypes message;
  message.set_single_int64(1);
  activation.InsertOrAssignValue("test_all_types", TestWrapMessage(&message));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;

  // has(test_all_types.single_string)
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<BoolValue>(result));
  EXPECT_FALSE(Cast<BoolValue>(result).NativeValue());
}

TEST_F(DirectSelectStepTest, SelectFromUnsupportedType) {
  cel::Activation activation;
  RuntimeOptions options;

  auto step = CreateDirectSelectStep(
      CreateDirectIdentStep("bool_val", -1), cel::StringValue("one"),
      /*test_only=*/false, -1,
      /*enable_wrapper_type_null_unboxing=*/true);

  activation.InsertOrAssignValue("bool_val", BoolValue(false));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));

  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Applying SELECT to non-message type")));
}

TEST_F(DirectSelectStepTest, AttributeUpdatedIfRequested) {
  cel::Activation activation;
  RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;

  auto step =
      CreateDirectSelectStep(CreateDirectIdentStep("test_all_types", -1),
                             cel::StringValue("single_int64"),
                             /*test_only=*/false, -1,
                             /*enable_wrapper_type_null_unboxing=*/true);

  TestAllTypes message;
  message.set_single_int64(1);
  activation.InsertOrAssignValue("test_all_types", TestWrapMessage(&message));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<IntValue>(result));
  EXPECT_EQ(Cast<IntValue>(result).NativeValue(), 1);

  ASSERT_OK_AND_ASSIGN(std::string attr_str, attr.attribute().AsString());
  EXPECT_EQ(attr_str, "test_all_types.single_int64");
}

TEST_F(DirectSelectStepTest, MissingAttributesToErrors) {
  cel::Activation activation;
  RuntimeOptions options;
  options.enable_missing_attribute_errors = true;

  auto step =
      CreateDirectSelectStep(CreateDirectIdentStep("test_all_types", -1),
                             cel::StringValue("single_int64"),
                             /*test_only=*/false, -1,
                             /*enable_wrapper_type_null_unboxing=*/true);

  TestAllTypes message;
  message.set_single_int64(1);
  activation.InsertOrAssignValue("test_all_types", TestWrapMessage(&message));
  activation.SetMissingPatterns({cel::AttributePattern(
      "test_all_types",
      {cel::AttributeQualifierPattern::OfString("single_int64")})});

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("test_all_types.single_int64")));
}

TEST_F(DirectSelectStepTest, IdentifiesUnknowns) {
  cel::Activation activation;
  RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;

  auto step =
      CreateDirectSelectStep(CreateDirectIdentStep("test_all_types", -1),
                             cel::StringValue("single_int64"),
                             /*test_only=*/false, -1,
                             /*enable_wrapper_type_null_unboxing=*/true);

  TestAllTypes message;
  message.set_single_int64(1);
  activation.InsertOrAssignValue("test_all_types", TestWrapMessage(&message));
  activation.SetUnknownPatterns({cel::AttributePattern(
      "test_all_types",
      {cel::AttributeQualifierPattern::OfString("single_int64")})});

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<UnknownValue>(result));

  EXPECT_THAT(AttributeStrings(Cast<UnknownValue>(result)),
              UnorderedElementsAre("test_all_types.single_int64"));
}

TEST_F(DirectSelectStepTest, ForwardErrorValue) {
  cel::Activation activation;
  RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;

  auto step = CreateDirectSelectStep(
      CreateConstValueDirectStep(cel::ErrorValue(absl::InternalError("test1")),
                                 -1),
      cel::StringValue("single_int64"),
      /*test_only=*/false, -1,
      /*enable_wrapper_type_null_unboxing=*/true);

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kInternal, HasSubstr("test1")));
}

TEST_F(DirectSelectStepTest, ForwardUnknownOperand) {
  cel::Activation activation;
  RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;

  AttributeSet attr_set({Attribute("attr", {AttributeQualifier::OfInt(0)})});
  auto step = CreateDirectSelectStep(
      CreateConstValueDirectStep(
          cel::UnknownValue(cel::Unknown(std::move(attr_set))), -1),
      cel::StringValue("single_int64"),
      /*test_only=*/false, -1,
      /*enable_wrapper_type_null_unboxing=*/true);

  TestAllTypes message;
  message.set_single_int64(1);
  activation.InsertOrAssignValue("test_all_types", TestWrapMessage(&message));

  ExecutionFrameBase frame(activation, options, type_provider_,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena_);

  Value result;
  AttributeTrail attr;
  ASSERT_THAT(step->Evaluate(frame, result, attr), IsOk());

  ASSERT_TRUE(InstanceOf<UnknownValue>(result));
  EXPECT_THAT(AttributeStrings(Cast<UnknownValue>(result)),
              UnorderedElementsAre("attr[0]"));
}

}  // namespace

}  // namespace google::api::expr::runtime
