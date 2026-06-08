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

#include "extensions/sets_functions.h"

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/function_adapter.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {
using google::api::expr::runtime::CelFunctionRegistry;
using google::api::expr::runtime::ConvertToRuntimeOptions;
using google::api::expr::runtime::InterpreterOptions;

namespace {

absl::StatusOr<Value> SetsContains(
    const ListValue& list, const ListValue& sublist,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  bool any_missing = false;
  CEL_RETURN_IF_ERROR(sublist.ForEach(
      [&](const Value& sublist_element) -> absl::StatusOr<bool> {
        CEL_ASSIGN_OR_RETURN(auto contains,
                             list.Contains(sublist_element, descriptor_pool,
                                           message_factory, arena));

        // Treat CEL error as missing
        any_missing =
            !contains->Is<BoolValue>() || !contains.GetBool().NativeValue();
        // The first false result will terminate the loop.
        return !any_missing;
      },
      descriptor_pool, message_factory, arena));
  return BoolValue(!any_missing);
}

absl::StatusOr<Value> SetsIntersects(
    const ListValue& list, const ListValue& sublist,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  bool exists = false;
  CEL_RETURN_IF_ERROR(list.ForEach(
      [&](const Value& list_element) -> absl::StatusOr<bool> {
        CEL_ASSIGN_OR_RETURN(auto contains,
                             sublist.Contains(list_element, descriptor_pool,
                                              message_factory, arena));
        // Treat contains return CEL error as false for the sake of
        // intersecting.
        exists = contains->Is<BoolValue>() && contains.GetBool().NativeValue();
        return !exists;
      },
      descriptor_pool, message_factory, arena));

  return BoolValue(exists);
}

absl::StatusOr<Value> SetsEquivalent(
    const ListValue& list, const ListValue& sublist,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(
      auto contains_sublist,
      SetsContains(list, sublist, descriptor_pool, message_factory, arena));
  if (contains_sublist.Is<BoolValue>() &&
      !contains_sublist.GetBool().NativeValue()) {
    return contains_sublist;
  }
  return SetsContains(sublist, list, descriptor_pool, message_factory, arena);
}

absl::Status RegisterSetsContainsFunction(FunctionRegistry& registry) {
  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Value>, const ListValue&,
          const ListValue&>::CreateDescriptor("sets.contains",
                                              /*receiver_style=*/false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&,
                            const ListValue&>::WrapFunction(SetsContains));
}

absl::Status RegisterSetsIntersectsFunction(FunctionRegistry& registry) {
  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Value>, const ListValue&,
          const ListValue&>::CreateDescriptor("sets.intersects",
                                              /*receiver_style=*/false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&,
                            const ListValue&>::WrapFunction(SetsIntersects));
}

absl::Status RegisterSetsEquivalentFunction(FunctionRegistry& registry) {
  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Value>, const ListValue&,
          const ListValue&>::CreateDescriptor("sets.equivalent",
                                              /*receiver_style=*/false),
      BinaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&,
                            const ListValue&>::WrapFunction(SetsEquivalent));
}

absl::Status RegisterSetsDecls(TypeCheckerBuilder& b) {
  ListType list_t(b.arena(), TypeParamType("T"));
  CEL_ASSIGN_OR_RETURN(
      auto decl,
      MakeFunctionDecl("sets.contains",
                       MakeOverloadDecl("list_sets_contains_list", BoolType(),
                                        list_t, list_t)));
  CEL_RETURN_IF_ERROR(b.AddFunction(decl));

  CEL_ASSIGN_OR_RETURN(
      decl, MakeFunctionDecl("sets.equivalent",
                             MakeOverloadDecl("list_sets_equivalent_list",
                                              BoolType(), list_t, list_t)));
  CEL_RETURN_IF_ERROR(b.AddFunction(decl));

  CEL_ASSIGN_OR_RETURN(
      decl, MakeFunctionDecl("sets.intersects",
                             MakeOverloadDecl("list_sets_intersects_list",
                                              BoolType(), list_t, list_t)));
  return b.AddFunction(decl);
}

}  // namespace

CheckerLibrary SetsCheckerLibrary() {
  return {.id = "cel.lib.ext.sets", .configure = RegisterSetsDecls};
}

absl::Status RegisterSetsFunctions(FunctionRegistry& registry,
                                   const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterSetsContainsFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterSetsIntersectsFunction(registry));
  CEL_RETURN_IF_ERROR(RegisterSetsEquivalentFunction(registry));
  return absl::OkStatus();
}

absl::Status RegisterSetsFunctions(CelFunctionRegistry* registry,
                                   const InterpreterOptions& options) {
  return RegisterSetsFunctions(registry->InternalGetRegistry(),
                               ConvertToRuntimeOptions(options));
}

}  // namespace cel::extensions
