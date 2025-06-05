// Copyright 2024 Google LLC
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

#include "conformance/service.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "google/api/expr/conformance/v1alpha1/conformance_service.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/eval.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/rpc/code.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "checker/optional.h"
#include "checker/standard_library.h"
#include "checker/type_checker_builder.h"
#include "common/ast.h"
#include "common/decl.h"
#include "common/expr.h"
#include "common/memory.h"
#include "common/source.h"
#include "common/type.h"
#include "common/value.h"
#include "conformance/value_conversion.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/transform_utility.h"
#include "extensions/bindings_ext.h"
#include "extensions/encoders.h"
#include "extensions/math_ext.h"
#include "extensions/math_ext_macros.h"
#include "extensions/proto_ext.h"
#include "extensions/protobuf/ast_converters.h"
#include "extensions/protobuf/enum_adapter.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/type_reflector.h"
#include "extensions/strings.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "parser/standard_macros.h"
#include "runtime/activation.h"
#include "runtime/constant_folding.h"
#include "runtime/managed_value_factory.h"
#include "runtime/optional_types.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "proto/test/v1/proto2/test_all_types_extensions.pb.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"


using ::cel::CreateStandardRuntimeBuilder;
using ::cel::FunctionDecl;
using ::cel::Runtime;
using ::cel::RuntimeOptions;
using ::cel::VariableDecl;
using ::cel::conformance_internal::FromConformanceValue;
using ::cel::conformance_internal::ToConformanceValue;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel::extensions::RegisterProtobufEnum;

using ::google::protobuf::Arena;

namespace google::api::expr::runtime {

namespace {

bool IsCelNamespace(const cel::Expr& target) {
  return target.has_ident_expr() && target.ident_expr().name() == "cel";
}

absl::optional<cel::Expr> CelBlockMacroExpander(cel::MacroExprFactory& factory,
                                                cel::Expr& target,
                                                absl::Span<cel::Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  cel::Expr& bindings_arg = args[0];
  if (!bindings_arg.has_list_expr()) {
    return factory.ReportErrorAt(
        bindings_arg, "cel.block requires the first arg to be a list literal");
  }
  return factory.NewCall("cel.@block", args);
}

absl::optional<cel::Expr> CelIndexMacroExpander(cel::MacroExprFactory& factory,
                                                cel::Expr& target,
                                                absl::Span<cel::Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  cel::Expr& index_arg = args[0];
  if (!index_arg.has_const_expr() || !index_arg.const_expr().has_int_value()) {
    return factory.ReportErrorAt(
        index_arg, "cel.index requires a single non-negative int constant arg");
  }
  int64_t index = index_arg.const_expr().int_value();
  if (index < 0) {
    return factory.ReportErrorAt(
        index_arg, "cel.index requires a single non-negative int constant arg");
  }
  return factory.NewIdent(absl::StrCat("@index", index));
}

absl::optional<cel::Expr> CelIterVarMacroExpander(
    cel::MacroExprFactory& factory, cel::Expr& target,
    absl::Span<cel::Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  cel::Expr& depth_arg = args[0];
  if (!depth_arg.has_const_expr() || !depth_arg.const_expr().has_int_value() ||
      depth_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        depth_arg, "cel.iterVar requires two non-negative int constant args");
  }
  cel::Expr& unique_arg = args[1];
  if (!unique_arg.has_const_expr() ||
      !unique_arg.const_expr().has_int_value() ||
      unique_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        unique_arg, "cel.iterVar requires two non-negative int constant args");
  }
  return factory.NewIdent(
      absl::StrCat("@it:", depth_arg.const_expr().int_value(), ":",
                   unique_arg.const_expr().int_value()));
}

absl::optional<cel::Expr> CelAccuVarMacroExpander(
    cel::MacroExprFactory& factory, cel::Expr& target,
    absl::Span<cel::Expr> args) {
  if (!IsCelNamespace(target)) {
    return absl::nullopt;
  }
  cel::Expr& depth_arg = args[0];
  if (!depth_arg.has_const_expr() || !depth_arg.const_expr().has_int_value() ||
      depth_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        depth_arg, "cel.accuVar requires two non-negative int constant args");
  }
  cel::Expr& unique_arg = args[1];
  if (!unique_arg.has_const_expr() ||
      !unique_arg.const_expr().has_int_value() ||
      unique_arg.const_expr().int_value() < 0) {
    return factory.ReportErrorAt(
        unique_arg, "cel.accuVar requires two non-negative int constant args");
  }
  return factory.NewIdent(
      absl::StrCat("@ac:", depth_arg.const_expr().int_value(), ":",
                   unique_arg.const_expr().int_value()));
}

absl::Status RegisterCelBlockMacros(cel::MacroRegistry& registry) {
  CEL_ASSIGN_OR_RETURN(auto block_macro,
                       cel::Macro::Receiver("block", 2, CelBlockMacroExpander));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(block_macro));
  CEL_ASSIGN_OR_RETURN(auto index_macro,
                       cel::Macro::Receiver("index", 1, CelIndexMacroExpander));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(index_macro));
  CEL_ASSIGN_OR_RETURN(
      auto iter_var_macro,
      cel::Macro::Receiver("iterVar", 2, CelIterVarMacroExpander));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(iter_var_macro));
  CEL_ASSIGN_OR_RETURN(
      auto accu_var_macro,
      cel::Macro::Receiver("accuVar", 2, CelAccuVarMacroExpander));
  CEL_RETURN_IF_ERROR(registry.RegisterMacro(accu_var_macro));
  return absl::OkStatus();
}

google::rpc::Code ToGrpcCode(absl::StatusCode code) {
  return static_cast<google::rpc::Code>(code);
}

using ConformanceServiceInterface =
    ::cel_conformance::ConformanceServiceInterface;

// Return a normalized raw expr for evaluation.
google::api::expr::v1alpha1::Expr ExtractExpr(
    const conformance::v1alpha1::EvalRequest& request) {
  const v1alpha1::Expr* expr = nullptr;

  // For now, discard type-check information if any.
  if (request.has_parsed_expr()) {
    expr = &request.parsed_expr().expr();
  } else if (request.has_checked_expr()) {
    expr = &request.checked_expr().expr();
  }
  google::api::expr::v1alpha1::Expr out;
  (out).MergeFrom(*expr);
  return out;
}

absl::StatusOr<cel::Type> FromConformanceType(
    google::protobuf::Arena* arena, const google::api::expr::v1alpha1::Type& type) {
  google::api::expr::v1alpha1::Type unversioned;
  if (!unversioned.MergeFromString(type.SerializeAsString())) {
    return absl::InternalError("Failed to convert from v1alpha1 type.");
  }
  return cel::conformance_internal::FromConformanceType(arena, unversioned);
}

absl::Status LegacyParse(const conformance::v1alpha1::ParseRequest& request,
                         conformance::v1alpha1::ParseResponse& response,
                         bool enable_optional_syntax) {
  if (request.cel_source().empty()) {
    return absl::InvalidArgumentError("no source code");
  }
  cel::ParserOptions options;
  options.enable_optional_syntax = enable_optional_syntax;
  cel::MacroRegistry macros;
  CEL_RETURN_IF_ERROR(cel::RegisterStandardMacros(macros, options));
  CEL_RETURN_IF_ERROR(cel::extensions::RegisterBindingsMacros(macros, options));
  CEL_RETURN_IF_ERROR(cel::extensions::RegisterMathMacros(macros, options));
  CEL_RETURN_IF_ERROR(cel::extensions::RegisterProtoMacros(macros, options));
  CEL_RETURN_IF_ERROR(RegisterCelBlockMacros(macros));
  CEL_ASSIGN_OR_RETURN(auto source, cel::NewSource(request.cel_source(),
                                                   request.source_location()));
  CEL_ASSIGN_OR_RETURN(auto parsed_expr,
                       parser::Parse(*source, macros, options));
  (*response.mutable_parsed_expr()).MergeFrom(parsed_expr);
  return absl::OkStatus();
}

class LegacyConformanceServiceImpl : public ConformanceServiceInterface {
 public:
  static absl::StatusOr<std::unique_ptr<LegacyConformanceServiceImpl>> Create(
      bool optimize, bool recursive) {
    static auto* constant_arena = new Arena();

    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::NestedTestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::NestedTestAllTypes>();
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::int32_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::test_all_types_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::repeated_test_all_types);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            int64_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_repeated_test_all_types);

    InterpreterOptions options;
    options.enable_qualified_type_identifiers = true;
    options.enable_timestamp_duration_overflow_errors = true;
    options.enable_heterogeneous_equality = true;
    options.enable_empty_wrapper_null_unboxing = true;
    options.enable_qualified_identifier_rewrites = true;

    if (optimize) {
      std::cerr << "Enabling optimizations" << std::endl;
      options.constant_folding = true;
      options.constant_arena = constant_arena;
    }

    if (recursive) {
      options.max_recursion_depth = 48;
    }

    std::unique_ptr<CelExpressionBuilder> builder =
        CreateCelExpressionBuilder(options);
    auto type_registry = builder->GetTypeRegistry();
    type_registry->Register(
        google::api::expr::test::v1::proto2::GlobalEnum_descriptor());
    type_registry->Register(
        google::api::expr::test::v1::proto3::GlobalEnum_descriptor());
    type_registry->Register(google::api::expr::test::v1::proto2::TestAllTypes::
                                NestedEnum_descriptor());
    type_registry->Register(google::api::expr::test::v1::proto3::TestAllTypes::
                                NestedEnum_descriptor());
    CEL_RETURN_IF_ERROR(
        RegisterBuiltinFunctions(builder->GetRegistry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterEncodersFunctions(
        builder->GetRegistry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterStringsFunctions(
        builder->GetRegistry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterMathExtensionFunctions(
        builder->GetRegistry(), options));

    return absl::WrapUnique(
        new LegacyConformanceServiceImpl(std::move(builder)));
  }

  void Parse(const conformance::v1alpha1::ParseRequest& request,
             conformance::v1alpha1::ParseResponse& response) override {
    auto status =
        LegacyParse(request, response, /*enable_optional_syntax=*/false);
    if (!status.ok()) {
      auto* issue = response.add_issues();
      issue->set_code(ToGrpcCode(status.code()));
      issue->set_message(status.message());
    }
  }

  void Check(const conformance::v1alpha1::CheckRequest& request,
             conformance::v1alpha1::CheckResponse& response) override {
    auto issue = response.add_issues();
    issue->set_message("Check is not supported");
    issue->set_code(google::rpc::Code::UNIMPLEMENTED);
  }

  absl::Status Eval(const conformance::v1alpha1::EvalRequest& request,
                    conformance::v1alpha1::EvalResponse& response) override {
    Arena arena;
    google::api::expr::v1alpha1::SourceInfo source_info;
    google::api::expr::v1alpha1::Expr expr = ExtractExpr(request);
    builder_->set_container(request.container());
    auto cel_expression_status =
        builder_->CreateExpression(&expr, &source_info);

    if (!cel_expression_status.ok()) {
      return absl::InternalError(cel_expression_status.status().ToString());
    }

    auto cel_expression = std::move(cel_expression_status.value());
    Activation activation;

    for (const auto& pair : request.bindings()) {
      auto* import_value = Arena::Create<google::api::expr::v1alpha1::Value>(&arena);
      (*import_value).MergeFrom(pair.second.value());
      auto import_status = ValueToCelValue(*import_value, &arena);
      if (!import_status.ok()) {
        return absl::InternalError(import_status.status().ToString());
      }
      activation.InsertValue(pair.first, import_status.value());
    }

    auto eval_status = cel_expression->Evaluate(activation, &arena);
    if (!eval_status.ok()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = eval_status.status().ToString();
      return absl::OkStatus();
    }

    CelValue result = eval_status.value();
    if (result.IsError()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(result.ErrorOrDie()->message());
    } else {
      google::api::expr::v1alpha1::Value export_value;
      auto export_status = CelValueToValue(result, &export_value);
      if (!export_status.ok()) {
        return absl::InternalError(export_status.ToString());
      }
      auto* result_value = response.mutable_result()->mutable_value();
      (*result_value).MergeFrom(export_value);
    }
    return absl::OkStatus();
  }

 private:
  explicit LegacyConformanceServiceImpl(
      std::unique_ptr<CelExpressionBuilder> builder)
      : builder_(std::move(builder)) {}

  std::unique_ptr<CelExpressionBuilder> builder_;
};

class ModernConformanceServiceImpl : public ConformanceServiceInterface {
 public:
  static absl::StatusOr<std::unique_ptr<ModernConformanceServiceImpl>> Create(
      bool optimize, bool use_arena, bool recursive) {
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::TestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto3::NestedTestAllTypes>();
    google::protobuf::LinkMessageReflection<
        google::api::expr::test::v1::proto2::NestedTestAllTypes>();
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::int32_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::test_all_types_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::repeated_test_all_types);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            int64_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_nested_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            nested_enum_ext);
    google::protobuf::LinkExtensionReflection(
        google::api::expr::test::v1::proto2::Proto2ExtensionScopedMessage::
            message_scoped_repeated_test_all_types);

    RuntimeOptions options;
    options.enable_qualified_type_identifiers = true;
    options.enable_timestamp_duration_overflow_errors = true;
    options.enable_heterogeneous_equality = true;
    options.enable_empty_wrapper_null_unboxing = true;
    if (recursive) {
      options.max_recursion_depth = 48;
    }

    return absl::WrapUnique(
        new ModernConformanceServiceImpl(options, use_arena, optimize));
  }

  absl::StatusOr<std::unique_ptr<const cel::Runtime>> Setup(
      absl::string_view container) {
    RuntimeOptions options(options_);
    options.container = std::string(container);
    CEL_ASSIGN_OR_RETURN(
        auto builder, CreateStandardRuntimeBuilder(
                          google::protobuf::DescriptorPool::generated_pool(), options));

    if (enable_optimizations_) {
      CEL_RETURN_IF_ERROR(cel::extensions::EnableConstantFolding(
          builder, constant_memory_manager_,
          google::protobuf::MessageFactory::generated_factory()));
    }
    CEL_RETURN_IF_ERROR(cel::EnableReferenceResolver(
        builder, cel::ReferenceResolverEnabled::kAlways));

    auto& type_registry = builder.type_registry();
    // Use linked pbs in the generated descriptor pool.
    type_registry.AddTypeProvider(
        std::make_unique<cel::extensions::ProtoTypeReflector>());
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry,
        google::api::expr::test::v1::proto2::GlobalEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry,
        google::api::expr::test::v1::proto3::GlobalEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry, google::api::expr::test::v1::proto2::TestAllTypes::
                           NestedEnum_descriptor()));
    CEL_RETURN_IF_ERROR(RegisterProtobufEnum(
        type_registry, google::api::expr::test::v1::proto3::TestAllTypes::
                           NestedEnum_descriptor()));

    CEL_RETURN_IF_ERROR(cel::extensions::EnableOptionalTypes(builder));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterEncodersFunctions(
        builder.function_registry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterStringsFunctions(
        builder.function_registry(), options));
    CEL_RETURN_IF_ERROR(cel::extensions::RegisterMathExtensionFunctions(
        builder.function_registry(), options));

    return std::move(builder).Build();
  }

  void Parse(const conformance::v1alpha1::ParseRequest& request,
             conformance::v1alpha1::ParseResponse& response) override {
    auto status =
        LegacyParse(request, response, /*enable_optional_syntax=*/true);
    if (!status.ok()) {
      auto* issue = response.add_issues();
      issue->set_code(ToGrpcCode(status.code()));
      issue->set_message(status.message());
    }
  }

  void Check(const conformance::v1alpha1::CheckRequest& request,
             conformance::v1alpha1::CheckResponse& response) override {
    auto status = DoCheck(&constant_arena_, request, response);
    if (!status.ok()) {
      auto* issue = response.add_issues();
      issue->set_code(ToGrpcCode(status.code()));
      issue->set_message(status.message());
    }
  }

  absl::Status Eval(const conformance::v1alpha1::EvalRequest& request,
                    conformance::v1alpha1::EvalResponse& response) override {
    google::protobuf::Arena arena;
    auto proto_memory_manager = ProtoMemoryManagerRef(&arena);
    cel::MemoryManagerRef memory_manager =
        (use_arena_ ? proto_memory_manager
                    : cel::MemoryManagerRef::ReferenceCounting());

    auto runtime_status = Setup(request.container());
    if (!runtime_status.ok()) {
      return absl::InternalError(runtime_status.status().ToString(
          absl::StatusToStringMode::kWithEverything));
    }
    std::unique_ptr<const cel::Runtime> runtime =
        std::move(runtime_status).value();

    auto program_status = Plan(*runtime, request);
    if (!program_status.ok()) {
      return absl::InternalError(program_status.status().ToString(
          absl::StatusToStringMode::kWithEverything));
    }
    std::unique_ptr<cel::TraceableProgram> program =
        std::move(program_status).value();
    cel::ManagedValueFactory value_factory(program->GetTypeProvider(),
                                           memory_manager);
    cel::Activation activation;

    for (const auto& pair : request.bindings()) {
      google::api::expr::v1alpha1::Value import_value;
      (import_value).MergeFrom(pair.second.value());
      auto import_status =
          FromConformanceValue(value_factory.get(), import_value);
      if (!import_status.ok()) {
        return absl::InternalError(import_status.status().ToString());
      }

      activation.InsertOrAssignValue(pair.first,
                                     std::move(import_status).value());
    }

    auto eval_status = program->Evaluate(activation, value_factory.get());
    if (!eval_status.ok()) {
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = eval_status.status().ToString(
          absl::StatusToStringMode::kWithEverything);
      return absl::OkStatus();
    }

    cel::Value result = eval_status.value();
    if (result->Is<cel::ErrorValue>()) {
      const absl::Status& error = result.GetError().NativeValue();
      *response.mutable_result()
           ->mutable_error()
           ->add_errors()
           ->mutable_message() = std::string(
          error.ToString(absl::StatusToStringMode::kWithEverything));
    } else {
      auto export_status = ToConformanceValue(value_factory.get(), result);
      if (!export_status.ok()) {
        return absl::InternalError(export_status.status().ToString(
            absl::StatusToStringMode::kWithEverything));
      }
      auto* result_value = response.mutable_result()->mutable_value();
      (*result_value).MergeFrom(*export_status);
    }
    return absl::OkStatus();
  }

 private:
  explicit ModernConformanceServiceImpl(const RuntimeOptions& options,
                                        bool use_arena,
                                        bool enable_optimizations)
      : options_(options),
        use_arena_(use_arena),
        enable_optimizations_(enable_optimizations),
        constant_memory_manager_(
            use_arena_ ? ProtoMemoryManagerRef(&constant_arena_)
                       : cel::MemoryManagerRef::ReferenceCounting()) {}

  static absl::Status DoCheck(
      google::protobuf::Arena* arena, const conformance::v1alpha1::CheckRequest& request,
      conformance::v1alpha1::CheckResponse& response) {
    google::api::expr::v1alpha1::ParsedExpr parsed_expr;

    (parsed_expr).MergeFrom(request.parsed_expr());

    CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Ast> ast,
                         cel::extensions::CreateAstFromParsedExpr(parsed_expr));

    absl::string_view location = parsed_expr.source_info().location();
    std::unique_ptr<cel::Source> source;
    if (absl::StartsWith(location, "Source: ")) {
      location = absl::StripPrefix(location, "Source: ");
      CEL_ASSIGN_OR_RETURN(source, cel::NewSource(location));
    }

    CEL_ASSIGN_OR_RETURN(cel::TypeCheckerBuilder builder,
                         cel::CreateTypeCheckerBuilder(
                             google::protobuf::DescriptorPool::generated_pool()));

    if (!request.no_std_env()) {
      CEL_RETURN_IF_ERROR(builder.AddLibrary(cel::StandardLibrary()));
      CEL_RETURN_IF_ERROR(builder.AddLibrary(cel::OptionalCheckerLibrary()));
    }

    for (const auto& decl : request.type_env()) {
      const auto& name = decl.name();
      if (decl.has_function()) {
        FunctionDecl fn_decl;
        fn_decl.set_name(name);
        for (const auto& overload_pb : decl.function().overloads()) {
          cel::OverloadDecl overload;
          overload.set_id(overload_pb.overload_id());
          if (overload_pb.is_instance_function()) {
            overload.set_member(true);
          }
          for (const auto& param : overload_pb.params()) {
            CEL_ASSIGN_OR_RETURN(auto param_type,
                                 FromConformanceType(arena, param.type()));
            overload.mutable_args().push_back(param_type);
          }

          CEL_RETURN_IF_ERROR(fn_decl.AddOverload(std::move(overload)));
        }
        CEL_RETURN_IF_ERROR(builder.AddFunction(std::move(fn_decl)));
      } else if (decl.has_ident()) {
        VariableDecl var_decl;
        var_decl.set_name(name);
        CEL_ASSIGN_OR_RETURN(auto var_type,
                             FromConformanceType(arena, decl.ident().type()));
        var_decl.set_type(var_type);
        CEL_RETURN_IF_ERROR(builder.AddVariable(var_decl));
      }
    }
    builder.set_container(request.container());

    CEL_ASSIGN_OR_RETURN(auto checker, std::move(builder).Build());

    CEL_ASSIGN_OR_RETURN(auto validation_result,
                         checker->Check(std::move(ast)));

    for (const auto& checker_issue : validation_result.GetIssues()) {
      auto* issue = response.add_issues();
      issue->set_code(ToGrpcCode(absl::StatusCode::kInvalidArgument));
      if (source) {
        issue->set_message(checker_issue.ToDisplayString(*source));
      } else {
        issue->set_message(checker_issue.message());
      }
    }

    const cel::Ast* checked_ast = validation_result.GetAst();
    if (!validation_result.IsValid() || checked_ast == nullptr) {
      return absl::OkStatus();
    }
    CEL_ASSIGN_OR_RETURN(
        google::api::expr::v1alpha1::CheckedExpr pb_checked_ast,
        cel::extensions::CreateCheckedExprFromAst(*validation_result.GetAst()));
    *response.mutable_checked_expr() = std::move(pb_checked_ast);
    return absl::OkStatus();
  }

  static absl::StatusOr<std::unique_ptr<cel::TraceableProgram>> Plan(
      const cel::Runtime& runtime,
      const conformance::v1alpha1::EvalRequest& request) {
    std::unique_ptr<cel::Ast> ast;
    if (request.has_parsed_expr()) {
      google::api::expr::v1alpha1::ParsedExpr unversioned;
      (unversioned).MergeFrom(request.parsed_expr());

      CEL_ASSIGN_OR_RETURN(ast, cel::extensions::CreateAstFromParsedExpr(
                                    std::move(unversioned)));

    } else if (request.has_checked_expr()) {
      google::api::expr::v1alpha1::CheckedExpr unversioned;
      (unversioned).MergeFrom(request.checked_expr());
      CEL_ASSIGN_OR_RETURN(ast, cel::extensions::CreateAstFromCheckedExpr(
                                    std::move(unversioned)));
    }
    if (ast == nullptr) {
      return absl::InternalError("no expression provided");
    }

    return runtime.CreateTraceableProgram(std::move(ast));
  }

  RuntimeOptions options_;
  bool use_arena_;
  bool enable_optimizations_;
  Arena constant_arena_;
  cel::MemoryManagerRef constant_memory_manager_;
};

}  // namespace

}  // namespace google::api::expr::runtime

namespace cel_conformance {

absl::StatusOr<std::unique_ptr<ConformanceServiceInterface>>
NewConformanceService(const ConformanceServiceOptions& options) {
  if (options.modern) {
    return google::api::expr::runtime::ModernConformanceServiceImpl::Create(
        options.optimize, options.arena, options.recursive);
  } else {
    return google::api::expr::runtime::LegacyConformanceServiceImpl::Create(
        options.optimize, options.recursive);
  }
}

}  // namespace cel_conformance
