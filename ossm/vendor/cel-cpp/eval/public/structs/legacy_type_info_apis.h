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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_INFO_APIS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_INFO_APIS_H_

#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/public/message_wrapper.h"
#include "google/protobuf/descriptor.h"

namespace google::api::expr::runtime {

// Forward declared to resolve cyclic dependency.
class LegacyTypeAccessApis;
class LegacyTypeMutationApis;

// Interface for providing type info from a user defined type (represented as a
// message).
//
// Provides ability to obtain field access apis, type info, and debug
// representation of a message.
//
// The message parameter may wrap a nullptr to request generic accessors /
// mutators for the TypeInfo instance if it is available.
//
// This is implemented as a separate class from LegacyTypeAccessApis to resolve
// cyclic dependency between CelValue (which needs to access these apis to
// provide DebugString and ObtainCelTypename) and LegacyTypeAccessApis (which
// needs to return CelValue type for field access).
class LegacyTypeInfoApis {
 public:
  struct FieldDescription {
    int number;
    absl::string_view name;
  };

  virtual ~LegacyTypeInfoApis() = default;

  // Return a debug representation of the wrapped message.
  virtual std::string DebugString(
      const MessageWrapper& wrapped_message) const = 0;

  // Return a reference to the typename for the wrapped message's type.
  // The CEL interpreter assumes that the typename is owned externally and will
  // outlive any CelValues created by the interpreter.
  virtual absl::string_view GetTypename(
      const MessageWrapper& wrapped_message) const = 0;

  virtual const google::protobuf::Descriptor* absl_nullable GetDescriptor(
      const MessageWrapper& wrapped_message [[maybe_unused]]) const {
    return nullptr;
  }

  // Return a pointer to the wrapped message's access api implementation.
  //
  // The CEL interpreter assumes that the returned pointer is owned externally
  // and will outlive any CelValues created by the interpreter.
  //
  // Nullptr signals that the value does not provide access apis. For field
  // access, the interpreter will treat this the same as accessing a field that
  // is not defined for the type.
  virtual const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const = 0;

  // Return a pointer to the wrapped message's mutation api implementation.
  //
  // The CEL interpreter assumes that the returned pointer is owned externally
  // and will outlive any CelValues created by the interpreter.
  //
  // Nullptr signals that the value does not provide mutation apis.
  virtual const LegacyTypeMutationApis* GetMutationApis(
      const MessageWrapper& wrapped_message [[maybe_unused]]) const {
    return nullptr;
  }

  // Return a description of the underlying field if defined.
  //
  // The underlying string is expected to remain valid as long as the
  // LegacyTypeInfoApis instance.
  virtual absl::optional<FieldDescription> FindFieldByName(
      absl::string_view name [[maybe_unused]]) const {
    return absl::nullopt;
  }
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_INFO_APIS_H_
