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

#include "common/type.h"

#include <string>

#include "absl/base/nullability.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/type_kind.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace common_internal {

struct TypeTypeData final {
  static TypeTypeData* Create(google::protobuf::Arena* absl_nonnull arena,
                              const Type& type) {
    return google::protobuf::Arena::Create<TypeTypeData>(arena, type);
  }

  explicit TypeTypeData(const Type& type) : type(type) {}

  TypeTypeData() = delete;
  TypeTypeData(const TypeTypeData&) = delete;
  TypeTypeData(TypeTypeData&&) = delete;
  TypeTypeData& operator=(const TypeTypeData&) = delete;
  TypeTypeData& operator=(TypeTypeData&&) = delete;

  const Type type;
};

}  // namespace common_internal

std::string TypeType::DebugString() const {
  std::string s(name());
  if (!GetParameters().empty()) {
    absl::StrAppend(&s, "(", TypeKindToString(GetParameters().front().kind()),
                    ")");
  }
  return s;
}

TypeType::TypeType(google::protobuf::Arena* absl_nonnull arena, const Type& parameter)
    : TypeType(common_internal::TypeTypeData::Create(arena, parameter)) {}

TypeParameters TypeType::GetParameters() const {
  if (data_) {
    return TypeParameters(absl::MakeConstSpan(&data_->type, 1));
  }
  return {};
}

Type TypeType::GetType() const {
  if (data_) {
    return data_->type;
  }
  return Type();
}

}  // namespace cel
