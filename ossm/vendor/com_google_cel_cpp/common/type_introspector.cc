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

#include "common/type_introspector.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/types/thread_compatible_type_introspector.h"

namespace cel {

namespace {

common_internal::BasicStructTypeField MakeBasicStructTypeField(
    absl::string_view name, Type type, int32_t number) {
  return common_internal::BasicStructTypeField(name, number, type);
}

struct FieldNameComparer {
  using is_transparent = void;

  bool operator()(const common_internal::BasicStructTypeField& lhs,
                  const common_internal::BasicStructTypeField& rhs) const {
    return (*this)(lhs.name(), rhs.name());
  }

  bool operator()(const common_internal::BasicStructTypeField& lhs,
                  absl::string_view rhs) const {
    return (*this)(lhs.name(), rhs);
  }

  bool operator()(absl::string_view lhs,
                  const common_internal::BasicStructTypeField& rhs) const {
    return (*this)(lhs, rhs.name());
  }

  bool operator()(absl::string_view lhs, absl::string_view rhs) const {
    return lhs < rhs;
  }
};

struct FieldNumberComparer {
  using is_transparent = void;

  bool operator()(const common_internal::BasicStructTypeField& lhs,
                  const common_internal::BasicStructTypeField& rhs) const {
    return (*this)(lhs.number(), rhs.number());
  }

  bool operator()(const common_internal::BasicStructTypeField& lhs,
                  int64_t rhs) const {
    return (*this)(lhs.number(), rhs);
  }

  bool operator()(int64_t lhs,
                  const common_internal::BasicStructTypeField& rhs) const {
    return (*this)(lhs, rhs.number());
  }

  bool operator()(int64_t lhs, int64_t rhs) const { return lhs < rhs; }
};

struct WellKnownType {
  WellKnownType(
      const Type& type,
      std::initializer_list<common_internal::BasicStructTypeField> fields)
      : type(type), fields_by_name(fields), fields_by_number(fields) {
    std::sort(fields_by_name.begin(), fields_by_name.end(),
              FieldNameComparer{});
    std::sort(fields_by_number.begin(), fields_by_number.end(),
              FieldNumberComparer{});
  }

  explicit WellKnownType(const Type& type) : WellKnownType(type, {}) {}

  Type type;
  // We use `2` as that accommodates most well known types.
  absl::InlinedVector<common_internal::BasicStructTypeField, 2> fields_by_name;
  absl::InlinedVector<common_internal::BasicStructTypeField, 2>
      fields_by_number;

  absl::optional<StructTypeField> FieldByName(absl::string_view name) const {
    // Basically `std::binary_search`.
    auto it = std::lower_bound(fields_by_name.begin(), fields_by_name.end(),
                               name, FieldNameComparer{});
    if (it == fields_by_name.end() || it->name() != name) {
      return absl::nullopt;
    }
    return *it;
  }

  absl::optional<StructTypeField> FieldByNumber(int64_t number) const {
    // Basically `std::binary_search`.
    auto it = std::lower_bound(fields_by_number.begin(), fields_by_number.end(),
                               number, FieldNumberComparer{});
    if (it == fields_by_number.end() || it->number() != number) {
      return absl::nullopt;
    }
    return *it;
  }
};

using WellKnownTypesMap = absl::flat_hash_map<absl::string_view, WellKnownType>;

const WellKnownTypesMap& GetWellKnownTypesMap() {
  static const WellKnownTypesMap* types = []() -> WellKnownTypesMap* {
    WellKnownTypesMap* types = new WellKnownTypesMap();
    types->insert_or_assign(
        "google.protobuf.BoolValue",
        WellKnownType{BoolWrapperType{},
                      {MakeBasicStructTypeField("value", BoolType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.Int32Value",
        WellKnownType{IntWrapperType{},
                      {MakeBasicStructTypeField("value", IntType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.Int64Value",
        WellKnownType{IntWrapperType{},
                      {MakeBasicStructTypeField("value", IntType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.UInt32Value",
        WellKnownType{UintWrapperType{},
                      {MakeBasicStructTypeField("value", UintType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.UInt64Value",
        WellKnownType{UintWrapperType{},
                      {MakeBasicStructTypeField("value", UintType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.FloatValue",
        WellKnownType{DoubleWrapperType{},
                      {MakeBasicStructTypeField("value", DoubleType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.DoubleValue",
        WellKnownType{DoubleWrapperType{},
                      {MakeBasicStructTypeField("value", DoubleType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.StringValue",
        WellKnownType{StringWrapperType{},
                      {MakeBasicStructTypeField("value", StringType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.BytesValue",
        WellKnownType{BytesWrapperType{},
                      {MakeBasicStructTypeField("value", BytesType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.Duration",
        WellKnownType{DurationType{},
                      {MakeBasicStructTypeField("seconds", IntType{}, 1),
                       MakeBasicStructTypeField("nanos", IntType{}, 2)}});
    types->insert_or_assign(
        "google.protobuf.Timestamp",
        WellKnownType{TimestampType{},
                      {MakeBasicStructTypeField("seconds", IntType{}, 1),
                       MakeBasicStructTypeField("nanos", IntType{}, 2)}});
    types->insert_or_assign(
        "google.protobuf.Value",
        WellKnownType{
            DynType{},
            {MakeBasicStructTypeField("null_value", NullType{}, 1),
             MakeBasicStructTypeField("number_value", DoubleType{}, 2),
             MakeBasicStructTypeField("string_value", StringType{}, 3),
             MakeBasicStructTypeField("bool_value", BoolType{}, 4),
             MakeBasicStructTypeField("struct_value", JsonMapType(), 5),
             MakeBasicStructTypeField("list_value", ListType{}, 6)}});
    types->insert_or_assign(
        "google.protobuf.ListValue",
        WellKnownType{ListType{},
                      {MakeBasicStructTypeField("values", ListType{}, 1)}});
    types->insert_or_assign(
        "google.protobuf.Struct",
        WellKnownType{JsonMapType(),
                      {MakeBasicStructTypeField("fields", JsonMapType(), 1)}});
    types->insert_or_assign(
        "google.protobuf.Any",
        WellKnownType{AnyType{},
                      {MakeBasicStructTypeField("type_url", StringType{}, 1),
                       MakeBasicStructTypeField("value", BytesType{}, 2)}});
    types->insert_or_assign("null_type", WellKnownType{NullType{}});
    types->insert_or_assign("google.protobuf.NullValue",
                            WellKnownType{NullType{}});
    types->insert_or_assign("bool", WellKnownType{BoolType{}});
    types->insert_or_assign("int", WellKnownType{IntType{}});
    types->insert_or_assign("uint", WellKnownType{UintType{}});
    types->insert_or_assign("double", WellKnownType{DoubleType{}});
    types->insert_or_assign("bytes", WellKnownType{BytesType{}});
    types->insert_or_assign("string", WellKnownType{StringType{}});
    types->insert_or_assign("list", WellKnownType{ListType{}});
    types->insert_or_assign("map", WellKnownType{MapType{}});
    types->insert_or_assign("type", WellKnownType{TypeType{}});
    return types;
  }();
  return *types;
}

}  // namespace

absl::StatusOr<absl::optional<Type>> TypeIntrospector::FindType(
    TypeFactory& type_factory, absl::string_view name) const {
  const auto& well_known_types = GetWellKnownTypesMap();
  if (auto it = well_known_types.find(name); it != well_known_types.end()) {
    return it->second.type;
  }
  return FindTypeImpl(type_factory, name);
}

absl::StatusOr<absl::optional<TypeIntrospector::EnumConstant>>
TypeIntrospector::FindEnumConstant(TypeFactory& type_factory,
                                   absl::string_view type,
                                   absl::string_view value) const {
  if (type == "google.protobuf.NullValue" && value == "NULL_VALUE") {
    return EnumConstant{NullType{}, "google.protobuf.NullValue", "NULL_VALUE",
                        0};
  }
  return FindEnumConstantImpl(type_factory, type, value);
}

absl::StatusOr<absl::optional<StructTypeField>>
TypeIntrospector::FindStructTypeFieldByName(TypeFactory& type_factory,
                                            absl::string_view type,
                                            absl::string_view name) const {
  const auto& well_known_types = GetWellKnownTypesMap();
  if (auto it = well_known_types.find(type); it != well_known_types.end()) {
    return it->second.FieldByName(name);
  }
  return FindStructTypeFieldByNameImpl(type_factory, type, name);
}

absl::StatusOr<absl::optional<Type>> TypeIntrospector::FindTypeImpl(
    TypeFactory&, absl::string_view) const {
  return absl::nullopt;
}

absl::StatusOr<absl::optional<TypeIntrospector::EnumConstant>>
TypeIntrospector::FindEnumConstantImpl(TypeFactory&, absl::string_view,
                                       absl::string_view) const {
  return absl::nullopt;
}

absl::StatusOr<absl::optional<StructTypeField>>
TypeIntrospector::FindStructTypeFieldByNameImpl(TypeFactory&, absl::string_view,
                                                absl::string_view) const {
  return absl::nullopt;
}

Shared<TypeIntrospector> NewThreadCompatibleTypeIntrospector(
    MemoryManagerRef memory_manager) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleTypeIntrospector>();
}

}  // namespace cel
