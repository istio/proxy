#include "eval/eval/container_access_step.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "base/builtins.h"
#include "base/type_provider.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "eval/public/unknown_set.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::cel::TypeProvider;
using ::cel::ast_internal::Expr;
using ::cel::ast_internal::SourceInfo;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::protobuf::Struct;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;

using TestParamType = std::tuple<bool, bool, bool>;

CelValue EvaluateAttributeHelper(
    google::protobuf::Arena* arena, CelValue container, CelValue key,
    bool use_recursive_impl, bool receiver_style, bool enable_unknown,
    const std::vector<CelAttributePattern>& patterns) {
  ExecutionPath path;

  Expr expr;
  SourceInfo source_info;
  auto& call = expr.mutable_call_expr();

  call.set_function(cel::builtin::kIndex);

  call.mutable_args().reserve(2);
  Expr& container_expr = (receiver_style) ? call.mutable_target()
                                          : call.mutable_args().emplace_back();
  Expr& key_expr = call.mutable_args().emplace_back();

  container_expr.mutable_ident_expr().set_name("container");
  key_expr.mutable_ident_expr().set_name("key");

  if (use_recursive_impl) {
    path.push_back(std::make_unique<WrappedDirectStep>(
        CreateDirectContainerAccessStep(CreateDirectIdentStep("container", 1),
                                        CreateDirectIdentStep("key", 2),
                                        /*enable_optional_types=*/false, 3),
        3));
  } else {
    path.push_back(
        std::move(CreateIdentStep(container_expr.ident_expr(), 1).value()));
    path.push_back(
        std::move(CreateIdentStep(key_expr.ident_expr(), 2).value()));
    path.push_back(std::move(CreateContainerAccessStep(call, 3).value()));
  }

  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  options.enable_heterogeneous_equality = false;
  CelExpressionFlatImpl cel_expr(
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     TypeProvider::Builtin(), options));
  Activation activation;

  activation.InsertValue("container", container);
  activation.InsertValue("key", key);

  activation.set_unknown_attribute_patterns(patterns);
  auto result = cel_expr.Evaluate(activation, arena);
  return *result;
}

class ContainerAccessStepTest : public ::testing::Test {
 protected:
  ContainerAccessStepTest() = default;

  void SetUp() override {}

  CelValue EvaluateAttribute(
      CelValue container, CelValue key, bool receiver_style,
      bool enable_unknown, bool use_recursive_impl = false,
      const std::vector<CelAttributePattern>& patterns = {}) {
    return EvaluateAttributeHelper(&arena_, container, key, receiver_style,
                                   enable_unknown, use_recursive_impl,
                                   patterns);
  }
  google::protobuf::Arena arena_;
};

class ContainerAccessStepUniformityTest
    : public ::testing::TestWithParam<TestParamType> {
 protected:
  ContainerAccessStepUniformityTest() = default;

  void SetUp() override {}

  bool receiver_style() {
    TestParamType params = GetParam();
    return std::get<0>(params);
  }

  bool enable_unknown() {
    TestParamType params = GetParam();
    return std::get<1>(params);
  }

  bool use_recursive_impl() {
    TestParamType params = GetParam();
    return std::get<2>(params);
  }

  // Helper method. Looks up in registry and tests comparison operation.
  CelValue EvaluateAttribute(
      CelValue container, CelValue key, bool receiver_style,
      bool enable_unknown, bool use_recursive_impl = false,
      const std::vector<CelAttributePattern>& patterns = {}) {
    return EvaluateAttributeHelper(&arena_, container, key, receiver_style,
                                   enable_unknown, use_recursive_impl,
                                   patterns);
  }
  google::protobuf::Arena arena_;
};

TEST_P(ContainerAccessStepUniformityTest, TestListIndexAccess) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateInt64(1),
                                      receiver_style(), enable_unknown());

  ASSERT_TRUE(result.IsInt64());
  ASSERT_EQ(result.Int64OrDie(), 2);
}

TEST_P(ContainerAccessStepUniformityTest, TestListIndexAccessOutOfBounds) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateInt64(0),
                                      receiver_style(), enable_unknown());

  ASSERT_TRUE(result.IsInt64());
  result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                             CelValue::CreateInt64(2), receiver_style(),
                             enable_unknown());

  ASSERT_TRUE(result.IsInt64());
  result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                             CelValue::CreateInt64(-1), receiver_style(),
                             enable_unknown());

  ASSERT_TRUE(result.IsError());
  result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                             CelValue::CreateInt64(3), receiver_style(),
                             enable_unknown());

  ASSERT_TRUE(result.IsError());
}

TEST_P(ContainerAccessStepUniformityTest, TestListIndexAccessNotAnInt) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateUint64(1),
                                      receiver_style(), enable_unknown());

  ASSERT_TRUE(result.IsError());
}

TEST_P(ContainerAccessStepUniformityTest, TestMapKeyAccess) {
  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  const std::string kKey2 = "testkey2";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");
  (*cel_struct.mutable_fields())[kKey1].set_string_value("value1");
  (*cel_struct.mutable_fields())[kKey2].set_string_value("value2");

  CelValue result = EvaluateAttribute(
      CelProtoWrapper::CreateMessage(&cel_struct, &arena_),
      CelValue::CreateString(&kKey0), receiver_style(), enable_unknown());

  ASSERT_TRUE(result.IsString());
  ASSERT_EQ(result.StringOrDie().value(), "value0");
}

TEST_P(ContainerAccessStepUniformityTest, TestBoolKeyType) {
  CelMapBuilder cel_map;
  ASSERT_OK(cel_map.Add(CelValue::CreateBool(true),
                        CelValue::CreateStringView("value_true")));

  CelValue result = EvaluateAttribute(CelValue::CreateMap(&cel_map),
                                      CelValue::CreateBool(true),
                                      receiver_style(), enable_unknown());

  ASSERT_THAT(result, test::IsCelString("value_true"));
}

TEST_P(ContainerAccessStepUniformityTest, TestMapKeyAccessNotFound) {
  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");

  CelValue result = EvaluateAttribute(
      CelProtoWrapper::CreateMessage(&cel_struct, &arena_),
      CelValue::CreateString(&kKey1), receiver_style(), enable_unknown());

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(),
              StatusIs(absl::StatusCode::kNotFound,
                       AllOf(HasSubstr("Key not found in map : "),
                             HasSubstr("testkey1"))));
}

TEST_F(ContainerAccessStepTest, TestInvalidReceiverCreateContainerAccessStep) {
  Expr expr;
  auto& call = expr.mutable_call_expr();
  call.set_function(cel::builtin::kIndex);
  Expr& container_expr = call.mutable_target();
  container_expr.mutable_ident_expr().set_name("container");

  call.mutable_args().reserve(2);
  Expr& key_expr = call.mutable_args().emplace_back();
  key_expr.mutable_ident_expr().set_name("key");

  Expr& extra_arg = call.mutable_args().emplace_back();
  extra_arg.mutable_const_expr().set_bool_value(true);
  EXPECT_THAT(CreateContainerAccessStep(call, 0).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid argument count")));
}

TEST_F(ContainerAccessStepTest, TestInvalidGlobalCreateContainerAccessStep) {
  Expr expr;
  auto& call = expr.mutable_call_expr();
  call.set_function(cel::builtin::kIndex);
  call.mutable_args().reserve(3);
  Expr& container_expr = call.mutable_args().emplace_back();
  container_expr.mutable_ident_expr().set_name("container");

  Expr& key_expr = call.mutable_args().emplace_back();
  key_expr.mutable_ident_expr().set_name("key");

  Expr& extra_arg = call.mutable_args().emplace_back();
  extra_arg.mutable_const_expr().set_bool_value(true);
  EXPECT_THAT(CreateContainerAccessStep(call, 0).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid argument count")));
}

TEST_F(ContainerAccessStepTest, TestListIndexAccessUnknown) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateInt64(1), true, true, {});

  ASSERT_TRUE(result.IsInt64());
  ASSERT_EQ(result.Int64OrDie(), 2);

  std::vector<CelAttributePattern> patterns = {CelAttributePattern(
      "container",
      {CreateCelAttributeQualifierPattern(CelValue::CreateInt64(1))})};

  result =
      EvaluateAttribute(CelValue::CreateList(&cel_list),
                        CelValue::CreateInt64(1), true, true, false, patterns);

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST_F(ContainerAccessStepTest, TestListUnknownKey) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  UnknownSet unknown_set;
  CelValue result =
      EvaluateAttribute(CelValue::CreateList(&cel_list),
                        CelValue::CreateUnknownSet(&unknown_set), true, true);

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST_F(ContainerAccessStepTest, TestMapInvalidKey) {
  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  const std::string kKey2 = "testkey2";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");
  (*cel_struct.mutable_fields())[kKey1].set_string_value("value1");
  (*cel_struct.mutable_fields())[kKey2].set_string_value("value2");

  CelValue result =
      EvaluateAttribute(CelProtoWrapper::CreateMessage(&cel_struct, &arena_),
                        CelValue::CreateDouble(1.0), true, true);

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid map key type: 'double'")));
}

TEST_F(ContainerAccessStepTest, TestMapUnknownKey) {
  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  const std::string kKey2 = "testkey2";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");
  (*cel_struct.mutable_fields())[kKey1].set_string_value("value1");
  (*cel_struct.mutable_fields())[kKey2].set_string_value("value2");

  UnknownSet unknown_set;
  CelValue result =
      EvaluateAttribute(CelProtoWrapper::CreateMessage(&cel_struct, &arena_),
                        CelValue::CreateUnknownSet(&unknown_set), true, true);

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST_F(ContainerAccessStepTest, TestUnknownContainer) {
  UnknownSet unknown_set;
  CelValue result = EvaluateAttribute(CelValue::CreateUnknownSet(&unknown_set),
                                      CelValue::CreateInt64(1), true, true);

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST_F(ContainerAccessStepTest, TestInvalidContainerType) {
  CelValue result = EvaluateAttribute(CelValue::CreateInt64(1),
                                      CelValue::CreateInt64(0), true, true);

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid container type: 'int")));
}

INSTANTIATE_TEST_SUITE_P(
    CombinedContainerTest, ContainerAccessStepUniformityTest,
    testing::Combine(/*receiver_style*/ testing::Bool(),
                     /*unknown_enabled*/ testing::Bool(),
                     /*use_recursive_impl*/ testing::Bool()));

class ContainerAccessHeterogeneousLookupsTest : public testing::Test {
 public:
  ContainerAccessHeterogeneousLookupsTest() {
    options_.enable_heterogeneous_equality = true;
    builder_ = CreateCelExpressionBuilder(options_);
  }

 protected:
  InterpreterOptions options_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  google::protobuf::Arena arena_;
  Activation activation_;
};

TEST_F(ContainerAccessHeterogeneousLookupsTest, DoubleMapKeyInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2}[1.0]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelInt64(2));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, DoubleMapKeyNotAnInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2}[1.1]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, DoubleMapKeyUint) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1u: 2u}[1.0]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelUint64(2));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, DoubleListIndex) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("[1, 2, 3][1.0]"));

  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelInt64(2));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, DoubleListIndexNotAnInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("[1, 2, 3][1.1]"));

  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

// treat uint as uint before trying coercion to signed int.
TEST_F(ContainerAccessHeterogeneousLookupsTest, UintKeyAsUint) {
  // TODO: Map creation should error here instead of permitting
  // mixed key types with equivalent values.
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1u: 2u, 1: 2}[1u]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelUint64(2));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, UintKeyAsInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2}[1u]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelInt64(2));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, IntKeyAsUint) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1u: 2u}[1]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelUint64(2));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, UintListIndex) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("[1, 2, 3][2u]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelInt64(3));
}

TEST_F(ContainerAccessHeterogeneousLookupsTest, StringKeyUnaffected) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2, '1': 3}['1']"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelInt64(3));
}

class ContainerAccessHeterogeneousLookupsDisabledTest : public testing::Test {
 public:
  ContainerAccessHeterogeneousLookupsDisabledTest() {
    options_.enable_heterogeneous_equality = false;
    builder_ = CreateCelExpressionBuilder(options_);
  }

 protected:
  InterpreterOptions options_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  google::protobuf::Arena arena_;
  Activation activation_;
};

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, DoubleMapKeyInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2}[1.0]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, DoubleMapKeyNotAnInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2}[1.1]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, DoubleMapKeyUint) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1u: 2u}[1.0]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, DoubleListIndex) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("[1, 2, 3][1.0]"));

  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest,
       DoubleListIndexNotAnInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("[1, 2, 3][1.1]"));

  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, UintKeyAsUint) {
  // TODO: Map creation should error here instead of permitting
  // mixed key types with equivalent values.
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1u: 2u, 1: 2}[1u]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelUint64(2));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, UintKeyAsInt) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2}[1u]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, IntKeyAsUint) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1u: 2u}[1]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, UintListIndex) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("[1, 2, 3][2u]"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelError(_));
}

TEST_F(ContainerAccessHeterogeneousLookupsDisabledTest, StringKeyUnaffected) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("{1: 2, '1': 3}['1']"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder_->CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       cel_expr->Evaluate(activation_, &arena_));

  EXPECT_THAT(result, test::IsCelInt64(3));
}

}  // namespace

}  // namespace google::api::expr::runtime
