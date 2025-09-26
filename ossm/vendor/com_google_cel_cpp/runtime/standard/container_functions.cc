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

#include "runtime/standard/container_functions.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "common/values/list_value_builder.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

absl::StatusOr<int64_t> MapSizeImpl(const MapValue& value) {
  return value.Size();
}

absl::StatusOr<int64_t> ListSizeImpl(const ListValue& value) {
  return value.Size();
}

// Concatenation for CelList type.
absl::StatusOr<ListValue> ConcatList(
    const ListValue& value1, const ListValue& value2,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(auto size1, value1.Size());
  if (size1 == 0) {
    return value2;
  }
  CEL_ASSIGN_OR_RETURN(auto size2, value2.Size());
  if (size2 == 0) {
    return value1;
  }

  // TODO(uncreated-issue/50): add option for checking lists have homogenous element
  // types and use a more specialized list type when possible.
  auto list_builder = NewListValueBuilder(arena);

  list_builder->Reserve(size1 + size2);

  for (size_t i = 0; i < size1; i++) {
    CEL_ASSIGN_OR_RETURN(
        Value elem, value1.Get(i, descriptor_pool, message_factory, arena));
    CEL_RETURN_IF_ERROR(list_builder->Add(std::move(elem)));
  }
  for (size_t i = 0; i < size2; i++) {
    CEL_ASSIGN_OR_RETURN(
        Value elem, value2.Get(i, descriptor_pool, message_factory, arena));
    CEL_RETURN_IF_ERROR(list_builder->Add(std::move(elem)));
  }

  return std::move(*list_builder).Build();
}

// AppendList will append the elements in value2 to value1.
//
// This call will only be invoked within comprehensions where `value1` is an
// intermediate result which cannot be directly assigned or co-mingled with a
// user-provided list.
absl::StatusOr<ListValue> AppendList(ListValue value1, const Value& value2) {
  // The `value1` object cannot be directly addressed and is an intermediate
  // variable. Once the comprehension completes this value will in effect be
  // treated as immutable.
  if (auto mutable_list_value =
          cel::common_internal::AsMutableListValue(value1);
      mutable_list_value) {
    CEL_RETURN_IF_ERROR(mutable_list_value->Append(value2));
    return value1;
  }
  return absl::InvalidArgumentError("Unexpected call to runtime list append.");
}
}  // namespace

absl::Status RegisterContainerFunctions(FunctionRegistry& registry,
                                        const RuntimeOptions& options) {
  // receiver style = true/false
  // Support both the global and receiver style size() for lists and maps.
  for (bool receiver_style : {true, false}) {
    CEL_RETURN_IF_ERROR(registry.Register(
        cel::UnaryFunctionAdapter<absl::StatusOr<int64_t>, const ListValue&>::
            CreateDescriptor(cel::builtin::kSize, receiver_style),
        UnaryFunctionAdapter<absl::StatusOr<int64_t>,
                             const ListValue&>::WrapFunction(ListSizeImpl)));

    CEL_RETURN_IF_ERROR(registry.Register(
        UnaryFunctionAdapter<absl::StatusOr<int64_t>, const MapValue&>::
            CreateDescriptor(cel::builtin::kSize, receiver_style),
        UnaryFunctionAdapter<absl::StatusOr<int64_t>,
                             const MapValue&>::WrapFunction(MapSizeImpl)));
  }

  if (options.enable_list_concat) {
    CEL_RETURN_IF_ERROR(registry.Register(
        BinaryFunctionAdapter<
            absl::StatusOr<Value>, const ListValue&,
            const ListValue&>::CreateDescriptor(cel::builtin::kAdd, false),
        BinaryFunctionAdapter<absl::StatusOr<Value>, const ListValue&,
                              const ListValue&>::WrapFunction(ConcatList)));
  }

  return registry.Register(
      BinaryFunctionAdapter<
          absl::StatusOr<ListValue>, ListValue,
          const Value&>::CreateDescriptor(cel::builtin::kRuntimeListAppend,
                                          false),
      BinaryFunctionAdapter<absl::StatusOr<ListValue>, ListValue,
                            const Value&>::WrapFunction(AppendList));
}

}  // namespace cel
