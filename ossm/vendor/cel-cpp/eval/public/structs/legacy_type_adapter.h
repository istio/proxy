// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Definitions for legacy type APIs to emulate the behavior of the new type
// system.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_ADPATER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_ADPATER_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/memory.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// Interface for mutation apis.
// Note: in the new type system, a type provider represents this by returning
// a cel::Type and cel::ValueManager for the type.
class LegacyTypeMutationApis {
 public:
  virtual ~LegacyTypeMutationApis() = default;

  // Return whether the type defines the given field.
  // TODO(uncreated-issue/3): This is only used to eagerly fail during the planning
  // phase. Check if it's safe to remove this behavior and fail at runtime.
  virtual bool DefinesField(absl::string_view field_name) const = 0;

  // Create a new empty instance of the type.
  // May return a status if the type is not possible to create.
  virtual absl::StatusOr<CelValue::MessageWrapper::Builder> NewInstance(
      cel::MemoryManagerRef memory_manager) const = 0;

  // Normalize special types to a native CEL value after building.
  // The interpreter guarantees that instance is uniquely owned by the
  // interpreter, and can be safely mutated.
  virtual absl::StatusOr<CelValue> AdaptFromWellKnownType(
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder instance) const = 0;

  // Set field on instance to value.
  // The interpreter guarantees that instance is uniquely owned by the
  // interpreter, and can be safely mutated.
  virtual absl::Status SetField(
      absl::string_view field_name, const CelValue& value,
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder& instance) const = 0;

  virtual absl::Status SetFieldByNumber(
      int64_t field_number [[maybe_unused]], const CelValue& value [[maybe_unused]],
      cel::MemoryManagerRef memory_manager [[maybe_unused]],
      CelValue::MessageWrapper::Builder& instance [[maybe_unused]]) const {
    return absl::UnimplementedError("SetFieldByNumber is not yet implemented");
  }
};

// Interface for access apis.
// Note: in new type system this is integrated into the StructValue (via
// dynamic dispatch to concrete implementations).
class LegacyTypeAccessApis {
 public:
  struct LegacyQualifyResult {
    // The possibly intermediate result of the select operation.
    CelValue value;
    // Number of qualifiers applied.
    int qualifier_count;
  };

  virtual ~LegacyTypeAccessApis() = default;

  // Return whether an instance of the type has field set to a non-default
  // value.
  virtual absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const = 0;

  // Access field on instance.
  virtual absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManagerRef memory_manager) const = 0;

  // Apply a series of select operations on the given instance.
  //
  // Each select qualifier may represent either a singular field access (
  // FieldSpecifier) or an index into a container (AttributeQualifier).
  //
  // The Qualify implementation should return an appropriate CelError when
  // intermediate fields or indexes are not found, or the given qualifier
  // doesn't apply to operand.
  //
  // A Status with a non-ok error code may be returned for other errors.
  // absl::StatusCode::kUnimplemented signals that Qualify is unsupported and
  // the evaluator should emulate the default select behavior.
  //
  // - presence_test controls whether to treat the call as a 'has' call,
  // returning
  //   whether the leaf field is set to a non-default value.
  virtual absl::StatusOr<LegacyQualifyResult> Qualify(
      absl::Span<const cel::SelectQualifier>,
      const CelValue::MessageWrapper& instance [[maybe_unused]], bool presence_test [[maybe_unused]],
      cel::MemoryManagerRef memory_manager [[maybe_unused]]) const {
    return absl::UnimplementedError("Qualify unsupported.");
  }

  // Interface for equality operator.
  // The interpreter will check that both instances report to be the same type,
  // but implementations should confirm that both instances are actually of the
  // same type.
  // If the two instances are of different type, return false. Otherwise,
  // return whether they are equal.
  // To conform to the CEL spec, message equality should follow the behavior of
  // MessageDifferencer::Equals.
  virtual bool IsEqualTo(const CelValue::MessageWrapper&,
                         const CelValue::MessageWrapper&) const {
    return false;
  }

  virtual std::vector<absl::string_view> ListFields(
      const CelValue::MessageWrapper& instance) const = 0;
};

// Type information about a legacy Struct type.
// Provides methods to the interpreter for interacting with a custom type.
//
// mutation_apis() provide equivalent behavior to a cel::Type and
// cel::ValueManager (resolved from a type name).
//
// access_apis() provide equivalent behavior to cel::StructValue accessors
// (virtual dispatch to a concrete implementation for accessing underlying
// values).
//
// This class is a simple wrapper around (nullable) pointers to the interface
// implementations. The underlying pointers are expected to be valid as long as
// the type provider that returned this object.
class LegacyTypeAdapter {
 public:
  LegacyTypeAdapter(const LegacyTypeAccessApis* access,
                    const LegacyTypeMutationApis* mutation)
      : access_apis_(access), mutation_apis_(mutation) {}

  // Apis for access for the represented type.
  // If null, access is not supported (this is an opaque type).
  const LegacyTypeAccessApis* access_apis() { return access_apis_; }

  // Apis for mutation for the represented type.
  // If null, mutation is not supported (this type cannot be created).
  const LegacyTypeMutationApis* mutation_apis() { return mutation_apis_; }

 private:
  const LegacyTypeAccessApis* access_apis_;
  const LegacyTypeMutationApis* mutation_apis_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_ADPATER_H_
