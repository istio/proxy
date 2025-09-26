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

ABSL_CONST_INIT const MapTypeData kDynDynMapTypeData = {
    .key_and_value = {DynType(), DynType()},
};

ABSL_CONST_INIT const MapTypeData kStringDynMapTypeData = {
    .key_and_value = {StringType(), DynType()},
};

}  // namespace

MapTypeData* absl_nonnull MapTypeData::Create(google::protobuf::Arena* absl_nonnull arena,
                                              const Type& key,
                                              const Type& value) {
  MapTypeData* data =
      ::new (arena->AllocateAligned(sizeof(MapTypeData), alignof(MapTypeData)))
          MapTypeData;
  data->key_and_value[0] = key;
  data->key_and_value[1] = value;
  return data;
}

}  // namespace common_internal

MapType::MapType() : MapType(&common_internal::kDynDynMapTypeData) {}

MapType::MapType(google::protobuf::Arena* absl_nonnull arena, const Type& key,
                 const Type& value)
    : MapType(key.IsDyn() && value.IsDyn()
                  ? &common_internal::kDynDynMapTypeData
                  : common_internal::MapTypeData::Create(arena, key, value)) {}

std::string MapType::DebugString() const {
  return absl::StrCat("map<", TypeKindToString(key().kind()), ", ",
                      TypeKindToString(value().kind()), ">");
}

TypeParameters MapType::GetParameters() const {
  ABSL_DCHECK_NE(data_, 0);
  if ((data_ & kBasicBit) == kBasicBit) {
    const auto* data = reinterpret_cast<const common_internal::MapTypeData*>(
        data_ & kPointerMask);
    return TypeParameters(data->key_and_value[0], data->key_and_value[1]);
  }
  if ((data_ & kProtoBit) == kProtoBit) {
    const auto* descriptor =
        reinterpret_cast<const google::protobuf::Descriptor*>(data_ & kPointerMask);
    return TypeParameters(Type::Field(descriptor->map_key()),
                          Type::Field(descriptor->map_value()));
  }
  return TypeParameters(Type(), Type());
}

Type MapType::GetKey() const {
  ABSL_DCHECK_NE(data_, 0);
  if ((data_ & kBasicBit) == kBasicBit) {
    return reinterpret_cast<const common_internal::MapTypeData*>(data_ &
                                                                 kPointerMask)
        ->key_and_value[0];
  }
  if ((data_ & kProtoBit) == kProtoBit) {
    return Type::Field(
        reinterpret_cast<const google::protobuf::Descriptor*>(data_ & kPointerMask)
            ->map_key());
  }
  return Type();
}

Type MapType::key() const { return GetKey(); }

Type MapType::GetValue() const {
  ABSL_DCHECK_NE(data_, 0);
  if ((data_ & kBasicBit) == kBasicBit) {
    return reinterpret_cast<const common_internal::MapTypeData*>(data_ &
                                                                 kPointerMask)
        ->key_and_value[1];
  }
  if ((data_ & kProtoBit) == kProtoBit) {
    return Type::Field(
        reinterpret_cast<const google::protobuf::Descriptor*>(data_ & kPointerMask)
            ->map_value());
  }
  return Type();
}

Type MapType::value() const { return GetValue(); }

MapType JsonMapType() {
  return MapType(&common_internal::kStringDynMapTypeData);
}

}  // namespace cel
