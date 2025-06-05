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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_H_

#include <cstdint>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/numeric/bits.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class Type;
class TypeParameters;

namespace common_internal {
struct MapTypeData;
}  // namespace common_internal

class MapType;

MapType JsonMapType();

class MapType final {
 private:
  static constexpr uintptr_t kBasicBit = 1;
  static constexpr uintptr_t kProtoBit = 2;
  static constexpr uintptr_t kBits = kBasicBit | kProtoBit;
  static constexpr uintptr_t kPointerMask = ~kBits;

 public:
  static constexpr TypeKind kKind = TypeKind::kMap;
  static constexpr absl::string_view kName = "map";

  MapType(absl::Nonnull<google::protobuf::Arena*> arena, const Type& key,
          const Type& value);

  // By default, this type is `map(dyn, dyn)`. Unless you can help it, you
  // should use a more specific map type.
  MapType();
  MapType(const MapType&) = default;
  MapType(MapType&&) = default;
  MapType& operator=(const MapType&) = default;
  MapType& operator=(MapType&&) = default;

  static TypeKind kind() { return kKind; }

  static absl::string_view name() { return kName; }

  std::string DebugString() const;

  TypeParameters GetParameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  ABSL_DEPRECATED("Use GetKey")
  Type key() const;

  Type GetKey() const;

  ABSL_DEPRECATED("Use GetValue")
  Type value() const;

  Type GetValue() const;

 private:
  friend class Type;
  friend MapType JsonMapType();

  explicit MapType(absl::Nonnull<const common_internal::MapTypeData*> data)
      : data_(reinterpret_cast<uintptr_t>(data) | kBasicBit) {
    ABSL_DCHECK_GE(absl::countr_zero(reinterpret_cast<uintptr_t>(data)), 2)
        << "alignment must be greater than 2";
  }

  explicit MapType(absl::Nonnull<const google::protobuf::Descriptor*> descriptor)
      : data_(reinterpret_cast<uintptr_t>(descriptor) | kProtoBit) {
    ABSL_DCHECK_GE(absl::countr_zero(reinterpret_cast<uintptr_t>(descriptor)),
                   2)
        << "alignment must be greater than 2";
    ABSL_DCHECK(descriptor->map_key() != nullptr);
    ABSL_DCHECK(descriptor->map_value() != nullptr);
  }

  uintptr_t data_;
};

bool operator==(const MapType& lhs, const MapType& rhs);

inline bool operator!=(const MapType& lhs, const MapType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const MapType& type);

inline std::ostream& operator<<(std::ostream& out, const MapType& type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_H_
