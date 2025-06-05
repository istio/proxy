// Copyright 2022 Google LLC
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

#include "eval/internal/cel_value_equal.h"

#include <cstdint>

#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "base/kind.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_value.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "internal/number.h"
#include "google/protobuf/arena.h"

namespace cel::interop_internal {

namespace {

using ::cel::internal::Number;
using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelMap;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::GetNumberFromCelValue;
using ::google::api::expr::runtime::LegacyTypeAccessApis;
using ::google::api::expr::runtime::LegacyTypeInfoApis;

// Forward declaration of the functors for generic equality operator.
// Equal defined between compatible types.
struct HeterogeneousEqualProvider {
  absl::optional<bool> operator()(const CelValue& lhs,
                                  const CelValue& rhs) const;
};

// Comparison template functions
template <class Type>
absl::optional<bool> Inequal(Type lhs, Type rhs) {
  return lhs != rhs;
}

template <class Type>
absl::optional<bool> Equal(Type lhs, Type rhs) {
  return lhs == rhs;
}

// Equality for lists. Template parameter provides either heterogeneous or
// homogenous equality for comparing members.
template <typename EqualsProvider>
absl::optional<bool> ListEqual(const CelList* t1, const CelList* t2) {
  if (t1 == t2) {
    return true;
  }
  int index_size = t1->size();
  if (t2->size() != index_size) {
    return false;
  }

  google::protobuf::Arena arena;
  for (int i = 0; i < index_size; i++) {
    CelValue e1 = (*t1).Get(&arena, i);
    CelValue e2 = (*t2).Get(&arena, i);
    absl::optional<bool> eq = EqualsProvider()(e1, e2);
    if (eq.has_value()) {
      if (!(*eq)) {
        return false;
      }
    } else {
      // Propagate that the equality is undefined.
      return eq;
    }
  }

  return true;
}

// Equality for maps. Template parameter provides either heterogeneous or
// homogenous equality for comparing values.
template <typename EqualsProvider>
absl::optional<bool> MapEqual(const CelMap* t1, const CelMap* t2) {
  if (t1 == t2) {
    return true;
  }
  if (t1->size() != t2->size()) {
    return false;
  }

  google::protobuf::Arena arena;
  auto list_keys = t1->ListKeys(&arena);
  if (!list_keys.ok()) {
    return absl::nullopt;
  }
  const CelList* keys = *list_keys;
  for (int i = 0; i < keys->size(); i++) {
    CelValue key = (*keys).Get(&arena, i);
    CelValue v1 = (*t1).Get(&arena, key).value();
    absl::optional<CelValue> v2 = (*t2).Get(&arena, key);
    if (!v2.has_value()) {
      auto number = GetNumberFromCelValue(key);
      if (!number.has_value()) {
        return false;
      }
      if (!key.IsInt64() && number->LosslessConvertibleToInt()) {
        CelValue int_key = CelValue::CreateInt64(number->AsInt());
        absl::optional<bool> eq = EqualsProvider()(key, int_key);
        if (eq.has_value() && *eq) {
          v2 = (*t2).Get(&arena, int_key);
        }
      }
      if (!key.IsUint64() && !v2.has_value() &&
          number->LosslessConvertibleToUint()) {
        CelValue uint_key = CelValue::CreateUint64(number->AsUint());
        absl::optional<bool> eq = EqualsProvider()(key, uint_key);
        if (eq.has_value() && *eq) {
          v2 = (*t2).Get(&arena, uint_key);
        }
      }
    }
    if (!v2.has_value()) {
      return false;
    }
    absl::optional<bool> eq = EqualsProvider()(v1, *v2);
    if (!eq.has_value() || !*eq) {
      // Shortcircuit on value comparison errors and 'false' results.
      return eq;
    }
  }

  return true;
}

bool MessageEqual(const CelValue::MessageWrapper& m1,
                  const CelValue::MessageWrapper& m2) {
  const LegacyTypeInfoApis* lhs_type_info = m1.legacy_type_info();
  const LegacyTypeInfoApis* rhs_type_info = m2.legacy_type_info();

  if (lhs_type_info->GetTypename(m1) != rhs_type_info->GetTypename(m2)) {
    return false;
  }

  const LegacyTypeAccessApis* accessor = lhs_type_info->GetAccessApis(m1);

  if (accessor == nullptr) {
    return false;
  }

  return accessor->IsEqualTo(m1, m2);
}

// Generic equality for CEL values of the same type.
// EqualityProvider is used for equality among members of container types.
template <class EqualityProvider>
absl::optional<bool> HomogenousCelValueEqual(const CelValue& t1,
                                             const CelValue& t2) {
  if (t1.type() != t2.type()) {
    return absl::nullopt;
  }
  switch (t1.type()) {
    case Kind::kNullType:
      return Equal<CelValue::NullType>(CelValue::NullType(),
                                       CelValue::NullType());
    case Kind::kBool:
      return Equal<bool>(t1.BoolOrDie(), t2.BoolOrDie());
    case Kind::kInt64:
      return Equal<int64_t>(t1.Int64OrDie(), t2.Int64OrDie());
    case Kind::kUint64:
      return Equal<uint64_t>(t1.Uint64OrDie(), t2.Uint64OrDie());
    case Kind::kDouble:
      return Equal<double>(t1.DoubleOrDie(), t2.DoubleOrDie());
    case Kind::kString:
      return Equal<CelValue::StringHolder>(t1.StringOrDie(), t2.StringOrDie());
    case Kind::kBytes:
      return Equal<CelValue::BytesHolder>(t1.BytesOrDie(), t2.BytesOrDie());
    case Kind::kDuration:
      return Equal<absl::Duration>(t1.DurationOrDie(), t2.DurationOrDie());
    case Kind::kTimestamp:
      return Equal<absl::Time>(t1.TimestampOrDie(), t2.TimestampOrDie());
    case Kind::kList:
      return ListEqual<EqualityProvider>(t1.ListOrDie(), t2.ListOrDie());
    case Kind::kMap:
      return MapEqual<EqualityProvider>(t1.MapOrDie(), t2.MapOrDie());
    case Kind::kCelType:
      return Equal<CelValue::CelTypeHolder>(t1.CelTypeOrDie(),
                                            t2.CelTypeOrDie());
    default:
      break;
  }
  return absl::nullopt;
}

absl::optional<bool> HeterogeneousEqualProvider::operator()(
    const CelValue& lhs, const CelValue& rhs) const {
  return CelValueEqualImpl(lhs, rhs);
}

}  // namespace

// Equal operator is defined for all types at plan time. Runtime delegates to
// the correct implementation for types or returns nullopt if the comparison
// isn't defined.
absl::optional<bool> CelValueEqualImpl(const CelValue& v1, const CelValue& v2) {
  if (v1.type() == v2.type()) {
    // Message equality is only defined if heterogeneous comparisons are enabled
    // to preserve the legacy behavior for equality.
    if (CelValue::MessageWrapper lhs, rhs;
        v1.GetValue(&lhs) && v2.GetValue(&rhs)) {
      return MessageEqual(lhs, rhs);
    }
    return HomogenousCelValueEqual<HeterogeneousEqualProvider>(v1, v2);
  }

  absl::optional<Number> lhs = GetNumberFromCelValue(v1);
  absl::optional<Number> rhs = GetNumberFromCelValue(v2);

  if (rhs.has_value() && lhs.has_value()) {
    return *lhs == *rhs;
  }

  // TODO: It's currently possible for the interpreter to create a
  // map containing an Error. Return no matching overload to propagate an error
  // instead of a false result.
  if (v1.IsError() || v1.IsUnknownSet() || v2.IsError() || v2.IsUnknownSet()) {
    return absl::nullopt;
  }

  return false;
}

}  // namespace cel::interop_internal
