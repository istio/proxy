#include <memory>
#include <string>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"
#include "google/protobuf/text_format.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::absl_testing::StatusIs;
using ::cel::expr::Expr;
using ::cel::expr::SourceInfo;
using ::google::protobuf::Arena;
using ::google::protobuf::TextFormat;

// Simple one parameter function that records the message argument it receives.
class RecordArgFunction : public CelFunction {
 public:
  explicit RecordArgFunction(const std::string& name,
                             std::vector<CelValue>* output)
      : CelFunction(
            CelFunctionDescriptor{name, false, {CelValue::Type::kMessage}}),
        output_(*output) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 1) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }
    output_.push_back(args.at(0));
    *result = CelValue::CreateBool(true);
    return absl::OkStatus();
  }

  std::vector<CelValue>& output_;
};

// Simple end-to-end test, which also serves as usage example.
TEST(EndToEndTest, SimpleOnePlusOne) {
  // AST CEL equivalent of "1+var"
  constexpr char kExpr0[] = R"(
    call_expr: <
      function: "_+_"
      args: <
        ident_expr: <
          name: "var"
        >
      >
      args: <
        const_expr: <
          int64_value: 1
        >
      >
    >
  )";

  Expr expr;
  SourceInfo source_info;
  TextFormat::ParseFromString(kExpr0, &expr);

  // Obtain CEL Expression builder.
  std::unique_ptr<CelExpressionBuilder> builder = CreateCelExpressionBuilder();

  // Builtin registration.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));
  Activation activation;

  // Bind value to "var" parameter.
  activation.InsertValue("var", CelValue::CreateInt64(1));

  Arena arena;

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 2);
}

// Simple end-to-end test, which also serves as usage example.
TEST(EndToEndTest, EmptyStringCompare) {
  // AST CEL equivalent of "var.string_value == '' && var.int64_value == 0"
  constexpr char kExpr0[] = R"(
    call_expr: <
      function: "_&&_"
      args: <
        call_expr: <
          function: "_==_"
          args: <
            select_expr: <
              operand: <
                ident_expr: <
                  name: "var"
                >
              >
              field: "string_value"
            >
          >
          args: <
            const_expr: <
              string_value: ""
            >
          >
        >
      >
      args: <
        call_expr: <
          function: "_==_"
          args: <
            select_expr: <
              operand: <
                ident_expr: <
                  name: "var"
                >
              >
              field: "int64_value"
            >
          >
          args: <
            const_expr: <
              int64_value: 0
            >
          >
        >
      >
    >
  )";

  Expr expr;
  SourceInfo source_info;
  TextFormat::ParseFromString(kExpr0, &expr);

  // Obtain CEL Expression builder.
  std::unique_ptr<CelExpressionBuilder> builder = CreateCelExpressionBuilder();

  // Builtin registration.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));
  Activation activation;

  // Bind value to "var" parameter.
  constexpr char kData[] = R"(
    string_value: ""
    int64_value: 0
  )";
  TestMessage data;
  TextFormat::ParseFromString(kData, &data);
  Arena arena;
  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&data, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST(EndToEndTest, NullLiteral) {
  // AST CEL equivalent of "Value{null_value: NullValue.NULL_VALUE}"
  constexpr char kExpr0[] = R"(
    struct_expr: <
      message_name: "Value"
      entries: <
        field_key: "null_value"
        value: <
          select_expr: <
            operand: <
              ident_expr: <
                name: "NullValue"
              >
            >
            field: "NULL_VALUE"
          >
        >
      >
    >
  )";

  Expr expr;
  SourceInfo source_info;
  TextFormat::ParseFromString(kExpr0, &expr);

  // Obtain CEL Expression builder.
  std::unique_ptr<CelExpressionBuilder> builder = CreateCelExpressionBuilder();
  builder->set_container("google.protobuf");

  // Builtin registration.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));
  Activation activation;
  Arena arena;
  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsNull());
}

// Equivalent to 'RecordArg(test_message)'
constexpr char kNullMessageHandlingExpr[] = R"pb(
  id: 1
  call_expr: <
    function: "RecordArg"
    args: <
      ident_expr: < name: "test_message" >
      id: 2
    >
  >
)pb";

TEST(EndToEndTest, StrictNullHandling) {
  InterpreterOptions options;

  Expr expr;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kNullMessageHandlingExpr, &expr));
  SourceInfo info;

  auto builder = CreateCelExpressionBuilder(options);
  std::vector<CelValue> extension_calls;
  ASSERT_OK(builder->GetRegistry()->Register(
      std::make_unique<RecordArgFunction>("RecordArg", &extension_calls)));

  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder->CreateExpression(&expr, &info));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("test_message", CelValue::CreateNull());

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));
  const CelError* result_value;
  ASSERT_TRUE(result.GetValue(&result_value)) << result.DebugString();
  EXPECT_THAT(*result_value,
              StatusIs(absl::StatusCode::kUnknown,
                       testing::HasSubstr("No matching overloads")));
}

TEST(EndToEndTest, OutOfRangeDurationConstant) {
  InterpreterOptions options;
  options.enable_timestamp_duration_overflow_errors = true;

  Expr expr;
  // Duration representable in absl::Duration, but out of range for CelValue
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"(
          call_expr {
          function: "type"
          args {
            const_expr {
              duration_value {
                seconds: 28552639587287040
              }
            }
          }
        })",
      &expr));
  SourceInfo info;

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder->CreateExpression(&expr, &info));

  Activation activation;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));
  const CelError* result_value;
  ASSERT_TRUE(result.GetValue(&result_value)) << result.DebugString();
  EXPECT_THAT(*result_value,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       testing::HasSubstr("Duration is out of range")));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
