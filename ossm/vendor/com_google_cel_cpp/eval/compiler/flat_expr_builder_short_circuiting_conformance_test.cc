// A collection of tests that confirm that short-circuit and non-short-circuit
// produce expressions with the same outputs.
#include <memory>

#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::Expr;
using ::testing::Eq;
using ::testing::SizeIs;

constexpr char kTwoLogicalOp[] = R"cel(
id: 1
call_expr {
  function: "$0"
  args {
    id: 2
    ident_expr {
      name: "var1",
    }
  }
  args {
    id: 3
    call_expr {
      function: "$0"
      args {
        id: 4
        ident_expr {
          name: "var2"
        }
      }
      args {
        id: 5
        ident_expr {
          name: "var3"
        }
      }
    }
  }
}
)cel";

constexpr char kTernaryExpr[] = R"cel(
id: 1
call_expr {
  function: "_?_:_"
  args {
    id: 2
    ident_expr {
      name: "cond"
    }
  }
  args {
    id: 3
    ident_expr {
      name: "arg1"
    }
  }
  args {
    id: 4
    ident_expr {
      name: "arg2"
    }
  }
})cel";

void BuildAndEval(CelExpressionBuilder* builder, const Expr& expr,
                  const Activation& activation, google::protobuf::Arena* arena,
                  CelValue* result) {
  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder->CreateExpression(&expr, nullptr));

  auto value = expression->Evaluate(activation, arena);
  ASSERT_OK(value);

  *result = *value;
}

class ShortCircuitingTest : public testing::TestWithParam<bool> {
 public:
  std::unique_ptr<CelExpressionBuilder> GetBuilder(
      bool enable_unknowns = false) {
    cel::RuntimeOptions options;
    options.short_circuiting = GetParam();
    if (enable_unknowns) {
      options.unknown_processing =
          cel::UnknownProcessingOptions::kAttributeAndFunction;
    }
    auto result = std::make_unique<CelExpressionBuilderFlatImpl>(options);
    return result;
  }
};

TEST_P(ShortCircuitingTest, BasicAnd) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      absl::Substitute(kTwoLogicalOp, builtin::kAnd), &expr));
  auto builder = GetBuilder();

  activation.InsertValue("var1", CelValue::CreateBool(true));
  activation.InsertValue("var2", CelValue::CreateBool(true));
  activation.InsertValue("var3", CelValue::CreateBool(false));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsBool());
  EXPECT_FALSE(result.BoolOrDie());

  ASSERT_TRUE(activation.RemoveValueEntry("var3"));
  activation.InsertValue("var3", CelValue::CreateBool(true));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST_P(ShortCircuitingTest, BasicOr) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      absl::Substitute(kTwoLogicalOp, builtin::kOr), &expr));
  auto builder = GetBuilder();

  activation.InsertValue("var1", CelValue::CreateBool(false));
  activation.InsertValue("var2", CelValue::CreateBool(false));
  activation.InsertValue("var3", CelValue::CreateBool(true));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());

  ASSERT_TRUE(activation.RemoveValueEntry("var3"));
  activation.InsertValue("var3", CelValue::CreateBool(false));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsBool());
  EXPECT_FALSE(result.BoolOrDie());
}

TEST_P(ShortCircuitingTest, ErrorAnd) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      absl::Substitute(kTwoLogicalOp, builtin::kAnd), &expr));
  auto builder = GetBuilder();
  absl::Status error = absl::InternalError("error");

  activation.InsertValue("var1", CelValue::CreateBool(true));
  activation.InsertValue("var2", CelValue::CreateError(&error));
  activation.InsertValue("var3", CelValue::CreateBool(false));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsBool());
  EXPECT_FALSE(result.BoolOrDie());

  ASSERT_TRUE(activation.RemoveValueEntry("var3"));
  activation.InsertValue("var3", CelValue::CreateBool(true));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(),
              Eq(absl::Status(absl::StatusCode::kInternal, "error")));
}

TEST_P(ShortCircuitingTest, ErrorOr) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      absl::Substitute(kTwoLogicalOp, builtin::kOr), &expr));
  auto builder = GetBuilder();
  absl::Status error = absl::InternalError("error");

  activation.InsertValue("var1", CelValue::CreateBool(false));
  activation.InsertValue("var2", CelValue::CreateError(&error));
  activation.InsertValue("var3", CelValue::CreateBool(true));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());

  ASSERT_TRUE(activation.RemoveValueEntry("var3"));
  activation.InsertValue("var3", CelValue::CreateBool(false));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(),
              Eq(absl::Status(absl::StatusCode::kInternal, "error")));
}

TEST_P(ShortCircuitingTest, UnknownAnd) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      absl::Substitute(kTwoLogicalOp, builtin::kAnd), &expr));
  auto builder = GetBuilder(/* enable_unknowns=*/true);
  absl::Status error = absl::InternalError("error");

  activation.set_unknown_attribute_patterns({CelAttributePattern("var1", {})});
  activation.InsertValue("var2", CelValue::CreateError(&error));
  activation.InsertValue("var3", CelValue::CreateBool(false));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsBool());
  EXPECT_FALSE(result.BoolOrDie());

  ASSERT_TRUE(activation.RemoveValueEntry("var3"));
  activation.InsertValue("var3", CelValue::CreateBool(true));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsUnknownSet());
  const UnknownAttributeSet& attrs =
      result.UnknownSetOrDie()->unknown_attributes();
  ASSERT_THAT(attrs, testing::SizeIs(1));
  EXPECT_THAT(attrs.begin()->variable_name(), testing::Eq("var1"));
}

TEST_P(ShortCircuitingTest, UnknownOr) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      absl::Substitute(kTwoLogicalOp, builtin::kOr), &expr));
  auto builder = GetBuilder(/* enable_unknowns=*/true);
  absl::Status error = absl::InternalError("error");

  activation.set_unknown_attribute_patterns({CelAttributePattern("var1", {})});
  activation.InsertValue("var2", CelValue::CreateError(&error));
  activation.InsertValue("var3", CelValue::CreateBool(true));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());

  ASSERT_TRUE(activation.RemoveValueEntry("var3"));
  activation.InsertValue("var3", CelValue::CreateBool(false));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsUnknownSet());
  const UnknownAttributeSet& attrs =
      result.UnknownSetOrDie()->unknown_attributes();
  ASSERT_THAT(attrs, testing::SizeIs(1));
  EXPECT_THAT(attrs.begin()->variable_name(), testing::Eq("var1"));
}

TEST_P(ShortCircuitingTest, BasicTernary) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTernaryExpr, &expr));
  auto builder = GetBuilder();

  activation.InsertValue("cond", CelValue::CreateBool(true));
  activation.InsertValue("arg1", CelValue::CreateUint64(1));
  activation.InsertValue("arg2", CelValue::CreateInt64(-1));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsUint64());
  EXPECT_EQ(result.Uint64OrDie(), 1);

  ASSERT_TRUE(activation.RemoveValueEntry("cond"));
  activation.InsertValue("cond", CelValue::CreateBool(false));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), -1);
}

TEST_P(ShortCircuitingTest, TernaryErrorHandling) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTernaryExpr, &expr));
  auto builder = GetBuilder();

  absl::Status error1 = absl::InternalError("error1");
  absl::Status error2 = absl::InternalError("error2");

  activation.InsertValue("cond", CelValue::CreateError(&error1));
  activation.InsertValue("arg1", CelValue::CreateError(&error2));
  activation.InsertValue("arg2", CelValue::CreateInt64(-1));

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsError());
  EXPECT_EQ(*result.ErrorOrDie(), error1);

  ASSERT_TRUE(activation.RemoveValueEntry("cond"));
  activation.InsertValue("cond", CelValue::CreateBool(false));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), -1);
}

TEST_P(ShortCircuitingTest, TernaryUnknownCondHandling) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTernaryExpr, &expr));
  auto builder = GetBuilder(/*enable_unknowns=*/true);

  absl::Status error = absl::InternalError("error1");

  activation.InsertValue("cond", CelValue::CreateBool(false));
  activation.InsertValue("arg1", CelValue::CreateError(&error));
  activation.InsertValue("arg2", CelValue::CreateInt64(-1));

  activation.set_unknown_attribute_patterns({CelAttributePattern("cond", {})});

  CelValue result;
  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));

  ASSERT_TRUE(result.IsUnknownSet());
  const auto& attrs = result.UnknownSetOrDie()->unknown_attributes();
  ASSERT_THAT(attrs, SizeIs(1));
  EXPECT_THAT(attrs.begin()->variable_name(), Eq("cond"));

  // Unknown branches are discarded if condition is unknown
  activation.set_unknown_attribute_patterns({CelAttributePattern("cond", {}),
                                             CelAttributePattern("arg1", {}),
                                             CelAttributePattern("arg2", {})});

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsUnknownSet());
  const auto& attrs2 = result.UnknownSetOrDie()->unknown_attributes();
  ASSERT_THAT(attrs2, SizeIs(1));
  EXPECT_THAT(attrs2.begin()->variable_name(), Eq("cond"));
}

TEST_P(ShortCircuitingTest, TernaryUnknownArgsHandling) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTernaryExpr, &expr));
  auto builder = GetBuilder(/*enable_unknowns=*/true);

  absl::Status error = absl::InternalError("error1");

  activation.InsertValue("cond", CelValue::CreateBool(false));
  activation.InsertValue("arg1", CelValue::CreateError(&error));
  activation.InsertValue("arg2", CelValue::CreateInt64(-1));

  // Unknown arg is discarded if condition chooses other branch.
  activation.set_unknown_attribute_patterns({CelAttributePattern("arg1", {})});

  CelValue result;

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), -1);

  // Branches won't merge if both are unknown.
  activation.set_unknown_attribute_patterns(
      {CelAttributePattern("arg1", {}), CelAttributePattern("arg2", {})});

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsUnknownSet());
  const auto& attrs3 = result.UnknownSetOrDie()->unknown_attributes();
  ASSERT_THAT(attrs3, SizeIs(1));
  EXPECT_EQ(attrs3.begin()->variable_name(), "arg2");
}

TEST_P(ShortCircuitingTest, TernaryUnknownAndErrorHandling) {
  Expr expr;
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kTernaryExpr, &expr));
  auto builder = GetBuilder(/*enable_unknowns=*/true);

  absl::Status error = absl::InternalError("error1");

  activation.InsertValue("cond", CelValue::CreateError(&error));
  activation.InsertValue("arg1", CelValue::CreateInt64(1));
  activation.InsertValue("arg2", CelValue::CreateInt64(-1));

  // Error cond discards args
  activation.set_unknown_attribute_patterns(
      {CelAttributePattern("arg1", {}), CelAttributePattern("arg2", {})});

  CelValue result;

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsError());
  EXPECT_EQ(*result.ErrorOrDie(), error);

  // Error arg discarded if condition unknown
  activation.set_unknown_attribute_patterns({CelAttributePattern("cond", {})});
  ASSERT_TRUE(activation.RemoveValueEntry("arg1"));
  activation.InsertValue("arg1", CelValue::CreateError(&error));

  ASSERT_NO_FATAL_FAILURE(
      BuildAndEval(builder.get(), expr, activation, &arena, &result));
  ASSERT_TRUE(result.IsUnknownSet());
  const auto& attrs = result.UnknownSetOrDie()->unknown_attributes();
  ASSERT_THAT(attrs, SizeIs(1));
  EXPECT_EQ(attrs.begin()->variable_name(), "cond");
}

const char* TestName(testing::TestParamInfo<bool> info) {
  if (info.param) {
    return "short_circuit_enabled";
  } else {
    return "short_circuit_disabled";
  }
}

INSTANTIATE_TEST_SUITE_P(Test, ShortCircuitingTest,
                         testing::Values(false, true), &TestName);

}  // namespace

}  // namespace google::api::expr::runtime
