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

#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

namespace common_internal {

namespace {

ABSL_CONST_INIT const ListTypeData kDynListTypeData;

}  // namespace

ListTypeData* absl_nonnull ListTypeData::Create(
    google::protobuf::Arena* absl_nonnull arena, const Type& element) {
  return ::new (arena->AllocateAligned(
      sizeof(ListTypeData), alignof(ListTypeData))) ListTypeData(element);
}

ListTypeData::ListTypeData(const Type& element) : element(element) {}

}  // namespace common_internal

ListType::ListType() : ListType(&common_internal::kDynListTypeData) {}

ListType::ListType(google::protobuf::Arena* absl_nonnull arena, const Type& element)
    : ListType(element.IsDyn()
                   ? &common_internal::kDynListTypeData
                   : common_internal::ListTypeData::Create(arena, element)) {}

std::string ListType::DebugString() const {
  return absl::StrCat("list<", TypeKindToString(GetElement().kind()), ">");
}

TypeParameters ListType::GetParameters() const {
  return TypeParameters(GetElement());
}

Type ListType::GetElement() const {
  ABSL_DCHECK_NE(data_, 0);
  if ((data_ & kBasicBit) == kBasicBit) {
    return reinterpret_cast<const common_internal::ListTypeData*>(data_ &
                                                                  kPointerMask)
        ->element;
  }
  if ((data_ & kProtoBit) == kProtoBit) {
    return common_internal::SingularMessageFieldType(
        reinterpret_cast<const google::protobuf::FieldDescriptor*>(data_ & kPointerMask));
  }
  return Type();
}

Type ListType::element() const { return GetElement(); }

}  // namespace cel
