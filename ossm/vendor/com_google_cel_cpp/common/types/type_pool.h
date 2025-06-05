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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_POOL_H_

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/die_if_null.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "common/types/function_type_pool.h"
#include "common/types/list_type_pool.h"
#include "common/types/map_type_pool.h"
#include "common/types/opaque_type_pool.h"
#include "common/types/type_type_pool.h"
#include "internal/string_pool.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel::common_internal {

// `TypePool` is a thread safe interning factory for complex types. All types
// are allocated using the provided `google::protobuf::Arena`.
class TypePool final {
 public:
  TypePool(absl::Nonnull<const google::protobuf::DescriptorPool*> descriptors
               ABSL_ATTRIBUTE_LIFETIME_BOUND,
           absl::Nonnull<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : descriptors_(ABSL_DIE_IF_NULL(descriptors)),  // Crash OK
        arena_(ABSL_DIE_IF_NULL(arena)),              // Crash OK
        strings_(arena_),
        functions_(arena_),
        lists_(arena_),
        maps_(arena_),
        opaques_(arena_),
        types_(arena_) {}

  TypePool(const TypePool&) = delete;
  TypePool(TypePool&&) = delete;
  TypePool& operator=(const TypePool&) = delete;
  TypePool& operator=(TypePool&&) = delete;

  StructType MakeStructType(absl::string_view name);

  FunctionType MakeFunctionType(const Type& result,
                                absl::Span<const Type> args);

  ListType MakeListType(const Type& element);

  MapType MakeMapType(const Type& key, const Type& value);

  OpaqueType MakeOpaqueType(absl::string_view name,
                            absl::Span<const Type> parameters);

  OptionalType MakeOptionalType(const Type& parameter);

  TypeParamType MakeTypeParamType(absl::string_view name);

  TypeType MakeTypeType(const Type& type);

 private:
  absl::string_view InternString(absl::string_view string);

  absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptors_;
  absl::Nonnull<google::protobuf::Arena*> const arena_;
  absl::Mutex strings_mutex_;
  internal::StringPool strings_ ABSL_GUARDED_BY(strings_mutex_);
  absl::Mutex functions_mutex_;
  FunctionTypePool functions_ ABSL_GUARDED_BY(functions_mutex_);
  absl::Mutex lists_mutex_;
  ListTypePool lists_ ABSL_GUARDED_BY(lists_mutex_);
  absl::Mutex maps_mutex_;
  MapTypePool maps_ ABSL_GUARDED_BY(maps_mutex_);
  absl::Mutex opaques_mutex_;
  OpaqueTypePool opaques_ ABSL_GUARDED_BY(opaques_mutex_);
  absl::Mutex types_mutex_;
  TypeTypePool types_ ABSL_GUARDED_BY(types_mutex_);
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_POOL_H_
