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

#include "common/types/type_pool.h"

#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "common/type.h"

namespace cel::common_internal {

StructType TypePool::MakeStructType(absl::string_view name) {
  ABSL_DCHECK(!IsWellKnownMessageType(name)) << name;
  if (ABSL_PREDICT_FALSE(name.empty())) {
    return StructType();
  }
  if (const auto* descriptor = descriptors_->FindMessageTypeByName(name);
      descriptor != nullptr) {
    return MessageType(descriptor);
  }
  return MakeBasicStructType(InternString(name));
}

FunctionType TypePool::MakeFunctionType(const Type& result,
                                        absl::Span<const Type> args) {
  absl::MutexLock lock(&functions_mutex_);
  return functions_.InternFunctionType(result, args);
}

ListType TypePool::MakeListType(const Type& element) {
  if (element.IsDyn()) {
    return ListType();
  }
  absl::MutexLock lock(&lists_mutex_);
  return lists_.InternListType(element);
}

MapType TypePool::MakeMapType(const Type& key, const Type& value) {
  if (key.IsDyn() && value.IsDyn()) {
    return MapType();
  }
  if (key.IsString() && value.IsDyn()) {
    return JsonMapType();
  }
  absl::MutexLock lock(&maps_mutex_);
  return maps_.InternMapType(key, value);
}

OpaqueType TypePool::MakeOpaqueType(absl::string_view name,
                                    absl::Span<const Type> parameters) {
  if (name == OptionalType::kName) {
    if (parameters.size() == 1 && parameters.front().IsDyn()) {
      return OptionalType();
    }
    name = OptionalType::kName;
  } else {
    name = InternString(name);
  }
  absl::MutexLock lock(&opaques_mutex_);
  return opaques_.InternOpaqueType(name, parameters);
}

OptionalType TypePool::MakeOptionalType(const Type& parameter) {
  return MakeOpaqueType(OptionalType::kName, absl::MakeConstSpan(&parameter, 1))
      .GetOptional();
}

TypeParamType TypePool::MakeTypeParamType(absl::string_view name) {
  return TypeParamType(InternString(name));
}

TypeType TypePool::MakeTypeType(const Type& type) {
  absl::MutexLock lock(&types_mutex_);
  return types_.InternTypeType(type);
}

absl::string_view TypePool::InternString(absl::string_view string) {
  absl::MutexLock lock(&strings_mutex_);
  return strings_.InternString(string);
}

}  // namespace cel::common_internal
