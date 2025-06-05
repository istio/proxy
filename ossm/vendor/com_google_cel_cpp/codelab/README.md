# What is CEL?
Common Expression Language (CEL) is an expression language that’s fast, portable, and safe to execute in performance-critical applications. CEL is designed to be embedded in an application, with application-specific extensions, and is ideal for extending declarative configurations that your applications might already use.

## What is covered in this Codelab?
This codelab is aimed at developers who would like to learn CEL to use services that already support CEL. This Codelab covers common use cases. This codelab doesn't cover how to integrate CEL into your own project. For a more in-depth look at the language, semantics, and features see the [CEL Language Definition on GitHub](https://github.com/google/cel-spec).

Some key areas covered are:

* [Hello, World: Using CEL to evaluate a String](#hello-world)
* [Creating variables](#creating-variables)
* [Commutative logical AND/OR](#logical-andor)
* [Adding custom functions](#custom-functions)

### Prerequisites
This codelab builds upon a basic understanding of Protocol Buffers and C++.

If you're not familiar with Protocol Buffers, the first exercise will give you a sense of how CEL works, but because the more advanced examples use Protocol Buffers as the input into CEL, they may be harder to understand. Consider working through one of these tutorials, first. See the devsite for [Protocol Buffers](https://protobuf.dev).

Note that Protocol Buffers are not required to use CEL, but they are used extensively in this codelab.

What you'll need:

- Git
- Bazel
- C/C++ Compiler (GCC, Clang, Visual Studio)

## GitHub Setup

GitHub Repo:

The code for this codelab lives in the `codelab` folder of the cel-cpp repo. The solution is available in the `codelab/solution` folder of the same repo.

Clone and cd into the repo:

```
git clone git@github.com:google/cel-cpp.git
cd cel-cpp
```

Make sure everything is working by building the codelab:

```
bazel build //codelab:all
```

## Hello, World
In the tried and true tradition of all programming languages, let's start with "Hello, World!".

Update exercise1.cc with the following:

Using declarations:

```c++
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelExpression;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
```

Implementation:

```c++
absl::StatusOr<std::string> ParseAndEvaluate(absl::string_view cel_expr)
{
  // === Start Codelab ===
  // Setup a default environment for building expressions.
  InterpreterOptions options;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  CEL_RETURN_IF_ERROR(
      RegisterBuiltinFunctions(builder->GetRegistry(), options));

  // Parse the expression. This is fine for codelabs, but this skips the type
  // checking phase. It won't check that functions and variables are available
  // in the environment, and it won't handle certain ambiguous identifier
  // expressions (e.g. container lookup vs namespaced name, packaged function
  // vs. receiver call style function).
  ParsedExpr parsed_expr;
  CEL_ASSIGN_OR_RETURN(parsed_expr, Parse(cel_expr));

  // The evaluator uses a proto Arena for incidental allocations during
  // evaluation.
  proto2::Arena arena;
  // The activation provides variables and functions that are bound into the
  // expression environment. In this example, there's no context expected, so
  // we just provide an empty one to the evaluator.
  Activation activation;

  // Build the expression plan. This assumes that the source expression AST and
  // the expression builder outlives the CelExpression object.
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<CelExpression> expression_plan,
                       builder->CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  // Actually run the expression plan. We don't support any environment
  // variables at the moment so just use an empty activation.
  CEL_ASSIGN_OR_RETURN(CelValue result,
                       expression_plan->Evaluate(activation, &arena));

  // Convert the result to a c++ string. CelValues may reference instances from
  // either the input expression, or objects allocated on the arena, so we need
  // to pass ownership (in this case by copying to a new instance and returning
  // that).
  return ConvertResult(result);
  // === End Codelab ===
}
```

Run the following to check your work:

```
bazel test //codelab:exercise1_test
```

You can add additional test cases or experiment with different return types.

Hello, World! Now, let's break down what's happening.


### Setup the Environment
CEL applications evaluate an expression against an environment.

The standard CEL environment supports all of the types, operators, functions, and macros defined within the language spec. The environment can be customized by providing options to disable macros, declare custom variables and functions, etc.

An ExpressionBuilder maintains C++ evaluation environment. This creates a builder with the standard environment.

```c++
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_options.h"
...
// Setup a default environment for building expressions.

// Breaking behavior changes and optional features are controlled by
// InterpreterOptions.
InterpreterOptions options;

// Environment used for planning and evaluating expressions is managed by an
// ExpressionBuilder.
std::unique_ptr<CelExpressionBuilder> builder =
    CreateCelExpressionBuilder(options);

// Add standard function bindings e.g. for +,-,==,||,&& operators.
// Custom functions (implementing the CelFunction interface) can be added to the
// registry similarly.
CEL_RETURN_IF_ERROR(
    RegisterBuiltinFunctions(builder->GetRegistry(), options));
```

### Parse
After the environment is configured, you can parse and check the expressions:

```c++
#include "google/api/expr/syntax.proto.h"
#include "parser/parser.h"
// ...
ASSIGN_OR_RETURN(google::api::expr::ParsedExpr parsed_expr, google::api::expr::parser::Parse(cel_expr));
```

The C++ parser is a stand-alone utility. It's not aware of the evaluation environment and does not perform any semantic checks on the expression. A status is returned if the input string isn't a syntactically valid CEL expression or if it exceeds the configured complexity limits (see cel::ParserOptions and default limits).

### Evaluate
After the expressions have been parsed and checked into an AST representation, it can be converted into an evaluable program whose function bindings and evaluation modes can be customized depending on the stack you are using.
Once a CEL expression is planned, it can be evaluated against an evaluation context (an activation). The evaluation result will be either a value or an error state.
The InterpreterOptions to create the expression plan are honored at evaluation. C++ uses the proto representation of either a parsed `google.api.expr.ParsedExpr` or parsed and type-checked `google.api.expr.CheckedExpr` AST directly.
Once a CEL program is planned (represented by a `google::api::expr::runtime::CelExpression`), it can be evaluated against an `google::api::expr::runtime::Activation`. The Activation provides per-evaluation bindings for variables and functions in the expression's environment.

```c++
#include "net/proto2/public/arena.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "parser/parser.h"
...
// The evaluator uses a proto Arena for incidental allocations during
// evaluation.
proto2::Arena arena;
// The activation provides variables and functions that are bound into the
// expression environment. In this example, there's no context expected, so
// we just provide an empty one to the evaluator.
Activation activation;

// Build the expression plan. This assumes that the source expression AST and
// the expression builder outlives the CelExpression object.
CEL_ASSIGN_OR_RETURN(std::unique_ptr<CelExpression> expression_plan,
                     builder->CreateExpression(&parsed_expr.expr(),
                                               &parsed_expr.source_info()));

// Actually run the expression plan. We don't support any environment
// variables at the moment so just use an empty activation.
CEL_ASSIGN_OR_RETURN(CelValue result,
                     expression_plan->Evaluate(activation, &arena));

// Convert the result to a C++ string. CelValues may reference instances from
// either the input expression, or objects allocated on the arena, so we need
// to pass ownership (in this case by copying to a new instance and returning
// that).
return ConvertResult(result);
```

## Creating variables
Most CEL applications will declare variables that can be referenced within expressions. Variables declarations specify a name and a type. A variable's type may either be a CEL builtin type, a protocol buffer well-known type, or any protobuf message type so long as its descriptor is also provided to CEL.

At runtime, the hosting program binds instances of variables to the evaluation context (using the variable name as a key).

For the C++ evaluator at runtime, the values are managed by the `google::api::expr::runtime::CelValue` type, a variant over the C++ representations of supported CEL types.

Update exercise2.cc:

```c++
// The Variables exercise shows how to declare and use variables in expressions.
// There are two overloads for preparing an expression either granularly for
// individual variables or using a helper to bind a context proto.

// The first overload shows manually populating individual variables in the
// evaluation environment. This allows cel_expr to reference 'bool_var'.
absl::StatusOr<bool> ParseAndEvaluate(absl::string_view cel_expr,
                                      bool bool_var) {
  Activation activation;
  proto2::Arena arena;
  // === Start Codelab ===
  activation.InsertValue("bool_var", CelValue::CreateBool(bool_var));
  // === End Codelab ===

  return ParseAndEvaluate(cel_expr, activation, &arena);
}
```

Run the following to check your work. You should have fixed the first two test cases in exercise2_test.cc.

```
bazel test //codelab:exercise2_test
```

The second overload uses a protocol buffer message to represent the environment variables. For this use case, there is a helper to automatically bind in fields from a top level message (see `google::api::expr::runtime::BindProtoToActivation`). In this example, we assume that unset fields should be bound to default values.

```c++
#include "eval/public/activation_bind_helper.h"
// ...
using ::google::api::expr::runtime::ProtoUnsetFieldOptions;
// ...
absl::StatusOr<bool> ParseAndEvaluate(absl::string_view cel_expr,
                                      const AttributeContext& context) {
  Activation activation;
  google::protobuf::Arena arena;
  // === Start Codelab ===

  CEL_RETURN_IF_ERROR(BindProtoToActivation(
      &context, &arena, &activation, ProtoUnsetFieldOptions::kBindDefault));
  // === End Codelab ===

  return ParseAndEvaluate(cel_expr, activation, &arena);
}
```

Note: You can experiment with unset values and the alternative bind option for BindProtoToActivation. With ProtoUnsetFieldOptions::kSkip unset values will not be bound at all, and accesses in expressions will cause errors.

## Logical And/Or
One of CEL's more distinctive features is its use of commutative logical operators. Either side of a conditional branch can short-circuit the evaluation, even in the face of errors or partial input.
Note: If you are skipping ahead, copy the solution for exercise2 -- we'll be using it to test the behavior of some simple expressions.

exercise3_test.cc lists truth tables for simple expressions using the 'or', 'and', and 'ternary' operators.

Running the following should result in some failing expectations.

```
bazel test //codelab:exercise3_test
```

Open exercise3_test.cc in your editor:

```c++
TEST(Exercise3Var, LogicalOr) {
  // Some of these expectations are incorrect.
  // If a logical operation can short-circuit a branch that results in an error,
  // CEL evaluation will return the logical result instead of propagating the
  // error. For logical or, this means if one branch is true, the result will
  // always be true, regardless of the other branch.
  // Wrong
  EXPECT_THAT(TruthTableTest("true || (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("false || (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  // Wrong
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) || true"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) || false"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) || (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("true || true"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("true || false"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("false || true"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("false || false"), IsOkAndHolds(false));
}
```

Updating the two failing cases "true || (1 / 0 > 2)" and "(1 / 0 > 2) || true" should fix this test:

```c++
// ...
  // Correct
  EXPECT_THAT(TruthTableTest("true || (1 / 0 > 2)"),
              IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("false || (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  // Correct
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) || true"),
              IsOkAndHolds(true));
```

You can examine the other tests for other cases for corresponding behavior for the 'and' and ternary operators.

CEL finds an evaluation order which gives results whenever possible, ignoring errors or even missing data that might occur in other evaluation orders. Applications like IAM conditions rely on this property to minimize the cost of evaluation, deferring the gathering of expensive inputs when a result can be reached without them.
