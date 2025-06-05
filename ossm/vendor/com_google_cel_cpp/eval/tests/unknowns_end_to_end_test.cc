// Integration tests for unknown processing in the C++ CEL runtime. The
// semantics of some of the tested expressions can be complicated because isn't
// possible to represent unknown values or errors directly in CEL -- declaring
// the unknowns is particular to the runtime.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "base/function_result.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/unknown_set.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::protobuf::Arena;
using ::testing::ElementsAre;

// var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
constexpr char kExprTextproto[] = R"pb(
  id: 13
  call_expr {
    function: "_||_"
    args {
      id: 6
      call_expr {
        function: "_&&_"
        args {
          id: 2
          call_expr {
            function: "_>_"
            args {
              id: 1
              ident_expr { name: "var1" }
            }
            args {
              id: 3
              const_expr { int64_value: 3 }
            }
          }
        }
        args {
          id: 4
          call_expr {
            function: "F1"
            args {
              id: 5
              const_expr { string_value: "arg1" }
            }
          }
        }
      }
    }
    args {
      id: 12
      call_expr {
        function: "_&&_"
        args {
          id: 8
          call_expr {
            function: "_>_"
            args {
              id: 7
              ident_expr { name: "var2" }
            }
            args {
              id: 9
              const_expr { int64_value: 3 }
            }
          }
        }
        args {
          id: 10
          call_expr {
            function: "F2"
            args {
              id: 11
              const_expr { string_value: "arg2" }
            }
          }
        }
      }
    }
  })pb";

enum class FunctionResponse { kUnknown, kTrue, kFalse };

CelFunctionDescriptor CreateDescriptor(
    absl::string_view name, CelValue::Type type = CelValue::Type::kString) {
  return CelFunctionDescriptor(std::string(name), false, {type});
}

class FunctionImpl : public CelFunction {
 public:
  FunctionImpl(absl::string_view name, FunctionResponse response,
               CelValue::Type type = CelValue::Type::kString)
      : CelFunction(CreateDescriptor(name, type)), response_(response) {}

  absl::Status Evaluate(absl::Span<const CelValue> arguments, CelValue* result,
                        Arena* arena) const override {
    switch (response_) {
      case FunctionResponse::kUnknown:
        *result = CreateUnknownFunctionResultError(arena, "help message");
        break;
      case FunctionResponse::kTrue:
        *result = CelValue::CreateBool(true);
        break;
      case FunctionResponse::kFalse:
        *result = CelValue::CreateBool(false);
        break;
    }
    return absl::OkStatus();
  }

 private:
  FunctionResponse response_;
};

// Text fixture for unknowns. Holds on to state needed for execution to work
// correctly.
class UnknownsTest : public testing::Test {
 public:
  void PrepareBuilder(UnknownProcessingOptions opts) {
    InterpreterOptions options;
    options.unknown_processing = opts;
    builder_ = CreateCelExpressionBuilder(options);
    ASSERT_OK(RegisterBuiltinFunctions(builder_->GetRegistry()));
    ASSERT_OK(
        builder_->GetRegistry()->RegisterLazyFunction(CreateDescriptor("F1")));
    ASSERT_OK(
        builder_->GetRegistry()->RegisterLazyFunction(CreateDescriptor("F2")));
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kExprTextproto, &expr_))
        << "error parsing expr";
  }

 protected:
  Arena arena_;
  Activation activation_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  google::api::expr::v1alpha1::Expr expr_;
};

MATCHER_P(FunctionCallIs, fn_name, "") {
  const cel::FunctionResult& result = arg;
  return result.descriptor().name() == fn_name;
}

MATCHER_P(AttributeIs, attr, "") {
  const cel::Attribute& result = arg;
  return result.variable_name() == attr;
}

TEST_F(UnknownsTest, NoUnknowns) {
  PrepareBuilder(UnknownProcessingOptions::kDisabled);

  activation_.InsertValue("var1", CelValue::CreateInt64(3));
  activation_.InsertValue("var2", CelValue::CreateInt64(5));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kFalse)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kTrue)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsBool());
  EXPECT_TRUE(response.BoolOrDie());
}

TEST_F(UnknownsTest, UnknownAttributes) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeOnly);
  activation_.set_unknown_attribute_patterns({CelAttributePattern("var1", {})});
  activation_.InsertValue("var2", CelValue::CreateInt64(3));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kTrue)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kFalse)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsUnknownSet());
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_attributes(),
              ElementsAre(AttributeIs("var1")));
}

TEST_F(UnknownsTest, UnknownAttributesPruning) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeOnly);
  activation_.set_unknown_attribute_patterns({CelAttributePattern("var1", {})});
  activation_.InsertValue("var2", CelValue::CreateInt64(5));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kTrue)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kTrue)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsBool());
  EXPECT_TRUE(response.BoolOrDie());
}

TEST_F(UnknownsTest, UnknownFunctionsWithoutOptionError) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeOnly);
  activation_.InsertValue("var1", CelValue::CreateInt64(5));
  activation_.InsertValue("var2", CelValue::CreateInt64(3));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kUnknown)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kFalse)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsError());
  EXPECT_EQ(response.ErrorOrDie()->code(), absl::StatusCode::kUnavailable);
}

TEST_F(UnknownsTest, UnknownFunctions) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);
  activation_.InsertValue("var1", CelValue::CreateInt64(5));
  activation_.InsertValue("var2", CelValue::CreateInt64(5));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kUnknown)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kFalse)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsUnknownSet()) << *response.ErrorOrDie();
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_function_results(),
              ElementsAre(FunctionCallIs("F1")));
}

TEST_F(UnknownsTest, UnknownsMerge) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);
  activation_.InsertValue("var1", CelValue::CreateInt64(5));
  activation_.set_unknown_attribute_patterns({CelAttributePattern("var2", {})});

  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F1", FunctionResponse::kUnknown)));
  ASSERT_OK(activation_.InsertFunction(
      std::make_unique<FunctionImpl>("F2", FunctionResponse::kTrue)));

  // var1 > 3 && F1('arg1') || var2 > 3 && F2('arg2')
  auto plan = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(plan);

  auto maybe_response = plan.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(maybe_response);
  CelValue response = maybe_response.value();

  ASSERT_TRUE(response.IsUnknownSet()) << *response.ErrorOrDie();
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_function_results(),
              ElementsAre(FunctionCallIs("F1")));
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_attributes(),
              ElementsAre(AttributeIs("var2")));
}

constexpr char kListCompExistsExpr[] = R"pb(
  id: 25
  comprehension_expr {
    iter_var: "x"
    iter_range {
      id: 1
      list_expr {
        elements {
          id: 2
          const_expr { int64_value: 1 }
        }
        elements {
          id: 3
          const_expr { int64_value: 2 }
        }
        elements {
          id: 4
          const_expr { int64_value: 3 }
        }
        elements {
          id: 5
          const_expr { int64_value: 4 }
        }
        elements {
          id: 6
          const_expr { int64_value: 5 }
        }
        elements {
          id: 7
          const_expr { int64_value: 6 }
        }
        elements {
          id: 8
          const_expr { int64_value: 7 }
        }
        elements {
          id: 9
          const_expr { int64_value: 8 }
        }
        elements {
          id: 10
          const_expr { int64_value: 9 }
        }
        elements {
          id: 11
          const_expr { int64_value: 10 }
        }
      }
    }
    accu_var: "__result__"
    accu_init {
      id: 18
      const_expr { bool_value: false }
    }
    loop_condition {
      id: 21
      call_expr {
        function: "@not_strictly_false"
        args {
          id: 20
          call_expr {
            function: "!_"
            args {
              id: 19
              ident_expr { name: "__result__" }
            }
          }
        }
      }
    }
    loop_step {
      id: 23
      call_expr {
        function: "_||_"
        args {
          id: 22
          ident_expr { name: "__result__" }
        }
        args {
          id: 16
          call_expr {
            function: "_>_"
            args {
              id: 14
              call_expr {
                function: "Fn"
                args {
                  id: 15
                  ident_expr { name: "x" }
                }
              }
            }
            args {
              id: 17
              const_expr { int64_value: 2 }
            }
          }
        }
      }
    }
    result {
      id: 24
      ident_expr { name: "__result__" }
    }
  })pb";

// Text fixture for comprehension tests. Holds on to state needed for execution
// to work correctly.
class UnknownsCompTest : public testing::Test {
 public:
  void PrepareBuilder(UnknownProcessingOptions opts) {
    InterpreterOptions options;
    options.unknown_processing = opts;
    builder_ = CreateCelExpressionBuilder(options);
    ASSERT_OK(RegisterBuiltinFunctions(builder_->GetRegistry()));
    ASSERT_OK(builder_->GetRegistry()->RegisterLazyFunction(
        CreateDescriptor("Fn", CelValue::Type::kInt64)));
    ASSERT_TRUE(
        google::protobuf::TextFormat::ParseFromString(kListCompExistsExpr, &expr_))
        << "error parsing expr";
  }

 protected:
  Arena arena_;
  Activation activation_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  Expr expr_;
};

TEST_F(UnknownsCompTest, UnknownsMerge) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);

  ASSERT_OK(activation_.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kUnknown, CelValue::Type::kInt64)));

  // [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].exists(x, Fn(x) > 5)
  auto build_status = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(build_status);

  auto eval_status = build_status.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(eval_status);
  CelValue response = eval_status.value();

  ASSERT_TRUE(response.IsUnknownSet()) << *response.ErrorOrDie();
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_function_results(),
              testing::SizeIs(1));
}

constexpr char kListCompCondExpr[] = R"pb(
  id: 25
  comprehension_expr {
    iter_var: "x"
    iter_range {
      id: 1
      list_expr {
        elements {
          id: 2
          const_expr { int64_value: 1 }
        }
        elements {
          id: 3

          const_expr { int64_value: 2 }

        }
        elements {
          id: 11
          const_expr { int64_value: 3 }
        }
      }
    }
    accu_var: "__result__"
    accu_init {
      id: 18
      const_expr { int64_value: 0 }
    }
    loop_condition {
      id: 21
      call_expr {
        function: "_<=_"
        args {
          id: 20
          ident_expr { name: "__result__" }
        }
        args {
          id: 19
          const_expr { int64_value: 1 }
        }
      }
    }
    loop_step {
      id: 23
      call_expr {
        function: "_?_:_"
        args {
          id: 22
          call_expr: {
            function: "Fn"
            args {
              id: 4
              ident_expr { name: "x" }
            }
          }
        }
        args {
          id: 14
          call_expr {
            function: "_+_"
            args {
              id: 15
              ident_expr { name: "__result__" }
            }
            args {
              id: 17
              const_expr { int64_value: 1 }
            }
          }
        }
        args {
          id: 16
          ident_expr { name: "__result__" }
        }
      }
    }
    result {
      id: 24
      call_expr {
        function: "_==_"
        args {
          id: 27
          ident_expr { name: "__result__" }
        }
        args {
          id: 26
          const_expr { int64_value: 1 }
        }
      }
    }
  })pb";

// Text fixture for comprehension tests affecting the condition step.
// Holds on to state needed for execution to work correctly.
class UnknownsCompCondTest : public testing::Test {
 public:
  void PrepareBuilder(UnknownProcessingOptions opts) {
    InterpreterOptions options;
    options.unknown_processing = opts;
    builder_ = CreateCelExpressionBuilder(options);
    ASSERT_OK(RegisterBuiltinFunctions(builder_->GetRegistry()));
    ASSERT_OK(builder_->GetRegistry()->RegisterLazyFunction(
        CreateDescriptor("Fn", CelValue::Type::kInt64)));
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kListCompCondExpr, &expr_))
        << "error parsing expr";
  }

 protected:
  Arena arena_;
  Activation activation_;
  std::unique_ptr<CelExpressionBuilder> builder_;
  Expr expr_;
};

TEST_F(UnknownsCompCondTest, UnknownConditionReturned) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);

  ASSERT_OK(activation_.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kUnknown, CelValue::Type::kInt64)));

  // [1, 2, 3].exists_one(x, Fn(x))
  auto build_status = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(build_status);

  auto eval_status = build_status.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(eval_status);
  CelValue response = eval_status.value();

  ASSERT_TRUE(response.IsUnknownSet()) << *response.ErrorOrDie();
  // The comprehension ends on the first non-bool condition, so we only get one
  // call captured in the UnknownSet.
  EXPECT_THAT(response.UnknownSetOrDie()->unknown_function_results(),
              testing::SizeIs(1));
}

TEST_F(UnknownsCompCondTest, ErrorConditionReturned) {
  PrepareBuilder(UnknownProcessingOptions::kAttributeAndFunction);

  // No implementation for Fn(int64_t) provided in activation -- this turns into a
  // CelError.
  // [1, 2, 3].exists_one(x, Fn(x))
  auto build_status = builder_->CreateExpression(&expr_, nullptr);
  ASSERT_OK(build_status);

  auto eval_status = build_status.value()->Evaluate(activation_, &arena_);
  ASSERT_OK(eval_status);
  CelValue response = eval_status.value();

  ASSERT_TRUE(response.IsError()) << CelValue::TypeName(response.type());
  EXPECT_TRUE(CheckNoMatchingOverloadError(response));
}

constexpr char kListCompExistsWithAttrExpr[] = R"pb(
  id: 25
  comprehension_expr {
    iter_var: "x"
    iter_range {
      id: 1
      ident_expr { name: "var" }
    }
    accu_var: "__result__"
    accu_init {
      id: 18
      const_expr { bool_value: false }
    }
    loop_condition {
      id: 21
      call_expr {
        function: "@not_strictly_false"
        args {
          id: 20
          call_expr {
            function: "!_"
            args {
              id: 19
              ident_expr { name: "__result__" }
            }
          }
        }
      }
    }
    loop_step {
      id: 23
      call_expr {
        function: "_||_"
        args {
          id: 22
          ident_expr { name: "__result__" }
        }
        args {
          id: 16
          call_expr {
            function: "Fn"
            args {
              id: 15
              ident_expr { name: "x" }
            }
          }
        }
      }
    }
    result {
      id: 24
      ident_expr { name: "__result__" }
    }
  })pb";

TEST(UnknownsIterAttrTest, IterAttributeTrail) {
  InterpreterOptions options;
  Expr expr;
  Activation activation;
  Arena arena;

  protobuf::Value element;
  protobuf::Value& value =
      element.mutable_struct_value()->mutable_fields()->operator[]("elem1");
  value.set_number_value(1);
  protobuf::ListValue list;
  *list.add_values() = element;
  *list.add_values() = element;
  *list.add_values() = element;

  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK(builder->GetRegistry()->RegisterLazyFunction(
      CreateDescriptor("Fn", CelValue::Type::kMap)));
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kListCompExistsWithAttrExpr, &expr))
      << "error parsing expr";

  // var.exists(x, Fn(x))
  auto plan = builder->CreateExpression(&expr, nullptr).value();

  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&list, &arena));

  // var[1]['elem1'] is unknown
  activation.set_unknown_attribute_patterns({CelAttributePattern(
      "var", {
                 CreateCelAttributeQualifierPattern(CelValue::CreateInt64(1)),
                 CreateCelAttributeQualifierPattern(
                     CelValue::CreateStringView("elem1")),
             })});

  ASSERT_OK(activation.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kFalse, CelValue::Type::kMap)));

  CelValue response = plan->Evaluate(activation, &arena).value();

  ASSERT_TRUE(response.IsUnknownSet()) << CelValue::TypeName(response.type());
  ASSERT_EQ(response.UnknownSetOrDie()->unknown_attributes().size(), 1);
  // 'var[1]' is partially unknown when we make the function call so we treat it
  // as unknown.
  ASSERT_EQ(response.UnknownSetOrDie()
                ->unknown_attributes()
                .begin()
                ->qualifier_path()
                .size(),
            1);
}

TEST(UnknownsIterAttrTest, IterAttributeTrailMapKeyTypes) {
  InterpreterOptions options;
  Expr expr;
  Activation activation;
  Arena arena;

  UnknownSet unknown_set;
  CelError error = absl::CancelledError();

  std::vector<std::pair<CelValue, CelValue>> backing;

  backing.push_back(
      {CelValue::CreateUnknownSet(&unknown_set), CelValue::CreateBool(false)});
  backing.push_back(
      {CelValue::CreateError(&error), CelValue::CreateBool(false)});
  backing.push_back({CelValue::CreateBool(true), CelValue::CreateBool(false)});

  auto map_impl = CreateContainerBackedMap(absl::MakeSpan(backing)).value();

  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK(builder->GetRegistry()->RegisterLazyFunction(
      CreateDescriptor("Fn", CelValue::Type::kBool)));
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kListCompExistsWithAttrExpr, &expr))
      << "error parsing expr";

  // var.exists(x, Fn(x))
  auto plan = builder->CreateExpression(&expr, nullptr).value();

  activation.InsertValue("var", CelValue::CreateMap(map_impl.get()));

  ASSERT_OK(activation.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kFalse, CelValue::Type::kBool)));

  CelValue response = plan->Evaluate(activation, &arena).value();

  ASSERT_TRUE(response.IsUnknownSet()) << CelValue::TypeName(response.type());
  ASSERT_EQ(*response.UnknownSetOrDie(), unknown_set);
}

TEST(UnknownsIterAttrTest, IterAttributeTrailMapKeyTypesShortcutted) {
  InterpreterOptions options;
  Expr expr;
  Activation activation;
  Arena arena;

  UnknownSet unknown_set;
  CelError error = absl::CancelledError();

  std::vector<std::pair<CelValue, CelValue>> backing;

  backing.push_back(
      {CelValue::CreateUnknownSet(&unknown_set), CelValue::CreateBool(false)});
  backing.push_back(
      {CelValue::CreateError(&error), CelValue::CreateBool(false)});
  backing.push_back({CelValue::CreateBool(true), CelValue::CreateBool(false)});

  auto map_impl = CreateContainerBackedMap(absl::MakeSpan(backing)).value();

  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK(builder->GetRegistry()->RegisterLazyFunction(
      CreateDescriptor("Fn", CelValue::Type::kBool)));
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kListCompExistsWithAttrExpr, &expr))
      << "error parsing expr";

  // var.exists(x, Fn(x))
  auto plan = builder->CreateExpression(&expr, nullptr).value();

  activation.InsertValue("var", CelValue::CreateMap(map_impl.get()));

  ASSERT_OK(activation.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kTrue, CelValue::Type::kBool)));

  CelValue response = plan->Evaluate(activation, &arena).value();
  ASSERT_TRUE(response.IsBool()) << CelValue::TypeName(response.type());
  ASSERT_TRUE(response.BoolOrDie());
}

constexpr char kMapElementsComp[] = R"pb(
  id: 25
  comprehension_expr {
    iter_var: "x"
    iter_range {
      id: 1
      ident_expr { name: "var" }
    }
    accu_var: "__result__"
    accu_init {
      id: 2
      list_expr {}
    }
    loop_condition {
      id: 3
      const_expr { bool_value: true }
    }
    loop_step {
      id: 4
      call_expr {
        function: "_+_"
        args {
          id: 5
          ident_expr { name: "__result__" }
        }
        args {
          id: 6
          list_expr {
            elements {
              id: 9
              call_expr {
                function: "Fn"
                args {
                  id: 7
                  select_expr {
                    field: "key"
                    operand {
                      id: 8
                      ident_expr { name: "x" }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    result {
      id: 9
      ident_expr { name: "__result__" }
    }
  })pb";

// TODO: Expected behavior for maps with unknown keys/values in a
// comprehension is a little unclear and the test coverage is a bit sparse.
// A few more tests should be added for coverage and to help document.
TEST(UnknownsIterAttrTest, IterAttributeTrailMap) {
  InterpreterOptions options;
  Expr expr;
  Activation activation;
  Arena arena;

  protobuf::Value element;
  protobuf::Value& value =
      element.mutable_struct_value()->mutable_fields()->operator[]("key");
  value.set_number_value(1);
  protobuf::ListValue list;
  *list.add_values() = element;
  *list.add_values() = element;
  *list.add_values() = element;

  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK(builder->GetRegistry()->RegisterLazyFunction(
      CreateDescriptor("Fn", CelValue::Type::kDouble)));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kMapElementsComp, &expr))
      << "error parsing expr";
  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&list, &arena));

  // var[1]['key'] is unknown
  activation.set_unknown_attribute_patterns({CelAttributePattern(
      "var",
      {
          CreateCelAttributeQualifierPattern(CelValue::CreateInt64(1)),
          CreateCelAttributeQualifierPattern(CelValue::CreateStringView("key")),
      })});

  ASSERT_OK(activation.InsertFunction(std::make_unique<FunctionImpl>(
      "Fn", FunctionResponse::kFalse, CelValue::Type::kDouble)));

  auto plan = builder->CreateExpression(&expr, nullptr).value();
  CelValue response = plan->Evaluate(activation, &arena).value();

  ASSERT_TRUE(response.IsUnknownSet()) << CelValue::TypeName(response.type());
  ASSERT_EQ(response.UnknownSetOrDie()->unknown_attributes().size(), 1);
  // 'var[1].key' is unknown when we make the Fn function call.
  // comprehension is:  ((([] + false) + unk) + false) -> unk
  ASSERT_EQ(response.UnknownSetOrDie()
                ->unknown_attributes()
                .begin()
                ->qualifier_path()
                .size(),
            2);
}

constexpr char kFilterElementsComp[] = R"pb(
  id: 25
  comprehension_expr {
    iter_var: "x"
    iter_range {
      id: 1
      ident_expr { name: "var" }
    }
    accu_var: "__result__"
    accu_init {
      id: 2
      list_expr {}
    }
    loop_condition {
      id: 3
      const_expr { bool_value: true }
    }
    loop_step {
      id: 4
      call_expr {
        function: "_?_:_"
        args {
          id: 5
          select_expr {
            field: "filter_key"
            operand {
              id: 6
              ident_expr { name: "x" }
            }
          }
        }
        args {
          id: 7
          call_expr {
            function: "_+_"
            args {
              id: 8
              ident_expr { name: "__result__" }
            }
            args {
              id: 9
              list_expr {
                elements {
                  id: 10
                  select_expr {
                    field: "value_key"
                    operand {
                      id: 12
                      ident_expr { name: "x" }
                    }
                  }
                }
              }
            }
          }
        }
        args {
          id: 13
          ident_expr { name: "__result__" }
        }
      }
    }
    result {
      id: 14
      ident_expr { name: "__result__" }
    }
  })pb";

TEST(UnknownsIterAttrTest, IterAttributeTrailExact) {
  InterpreterOptions options;
  Activation activation;
  Arena arena;

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, Parse("list_var.exists(x, x)"));

  protobuf::Value element;
  element.set_bool_value(false);
  protobuf::ListValue list;
  *list.add_values() = element;
  *list.add_values() = element;
  *list.add_values() = element;

  (*list.mutable_values())[0].set_bool_value(true);

  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  activation.InsertValue("list_var",
                         CelProtoWrapper::CreateMessage(&list, &arena));

  // list_var[0]
  std::vector<CelAttributePattern> unknown_attribute_patterns;
  unknown_attribute_patterns.push_back(CelAttributePattern(
      "list_var",
      {CreateCelAttributeQualifierPattern(CelValue::CreateInt64(0))}));
  activation.set_unknown_attribute_patterns(
      std::move(unknown_attribute_patterns));

  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));
  CelValue response = plan->Evaluate(activation, &arena).value();

  ASSERT_TRUE(response.IsUnknownSet()) << CelValue::TypeName(response.type());
  ASSERT_EQ(response.UnknownSetOrDie()->unknown_attributes().size(), 1);

  ASSERT_EQ(response.UnknownSetOrDie()
                ->unknown_attributes()
                .begin()
                ->qualifier_path()
                .size(),
            1);
}

TEST(UnknownsIterAttrTest, IterAttributeTrailFilterValues) {
  InterpreterOptions options;
  Expr expr;
  Activation activation;
  Arena arena;

  protobuf::Value element;
  protobuf::Value* value =
      &element.mutable_struct_value()->mutable_fields()->operator[](
          "filter_key");
  value->set_bool_value(true);
  value = &element.mutable_struct_value()->mutable_fields()->operator[](
      "value_key");
  value->set_number_value(1.0);
  protobuf::ListValue list;
  *list.add_values() = element;
  *list.add_values() = element;
  *list.add_values() = element;

  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kFilterElementsComp, &expr))
      << "error parsing expr";
  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&list, &arena));

  // var[1]['value_key'] is unknown
  activation.set_unknown_attribute_patterns({CelAttributePattern(
      "var", {
                 CreateCelAttributeQualifierPattern(CelValue::CreateInt64(1)),
                 CreateCelAttributeQualifierPattern(
                     CelValue::CreateStringView("value_key")),
             })});

  auto plan = builder->CreateExpression(&expr, nullptr).value();
  CelValue response = plan->Evaluate(activation, &arena).value();

  ASSERT_TRUE(response.IsUnknownSet()) << CelValue::TypeName(response.type());
  ASSERT_EQ(response.UnknownSetOrDie()->unknown_attributes().size(), 1);
  // 'var[1].value_key' is unknown when we make the cons function call.
  // comprehension is:  ((([] + [1]) + unk) + [1]) -> unk
  ASSERT_EQ(response.UnknownSetOrDie()
                ->unknown_attributes()
                .begin()
                ->qualifier_path()
                .size(),
            2);
}

TEST(UnknownsIterAttrTest, IterAttributeTrailFilterConditions) {
  InterpreterOptions options;
  Expr expr;
  Activation activation;
  Arena arena;

  protobuf::Value element;
  protobuf::Value* value =
      &element.mutable_struct_value()->mutable_fields()->operator[](
          "filter_key");
  value->set_bool_value(true);
  value = &element.mutable_struct_value()->mutable_fields()->operator[](
      "value_key");
  value->set_number_value(1.0);
  protobuf::ListValue list;
  *list.add_values() = element;
  *list.add_values() = element;
  *list.add_values() = element;

  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kFilterElementsComp, &expr))
      << "error parsing expr";
  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&list, &arena));

  // var[1]['value_key'] is unknown
  activation.set_unknown_attribute_patterns(
      {CelAttributePattern(
           "var",
           {
               CreateCelAttributeQualifierPattern(CelValue::CreateInt64(1)),
               CreateCelAttributeQualifierPattern(
                   CelValue::CreateStringView("filter_key")),
           }),
       CelAttributePattern(
           "var",
           {
               CreateCelAttributeQualifierPattern(CelValue::CreateInt64(0)),
               CreateCelAttributeQualifierPattern(
                   CelValue::CreateStringView("filter_key")),
           })});

  auto plan = builder->CreateExpression(&expr, nullptr).value();
  CelValue response = plan->Evaluate(activation, &arena).value();

  // 'var[1].filter_key' is unknown when we make the ternary call.
  // Since the unknown is expressed in a conditional jump, the behavior is to
  // ignore the possible outcomes
  // loop0: (unk{0})? [] + [1] : [] -> unk{0}
  // loop1: (unk{1})? unk{0} + [1] : unk{0} -> unk{1}
  // loop2: (true)? unk{1} + [1] : unk{1} -> unk{1}
  // result: unk{1}
  ASSERT_TRUE(response.IsUnknownSet()) << CelValue::TypeName(response.type());
  ASSERT_EQ(response.UnknownSetOrDie()->unknown_attributes().size(), 1);
  ASSERT_EQ(response.UnknownSetOrDie()
                ->unknown_attributes()
                .begin()
                ->qualifier_path()
                .size(),
            2);
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
