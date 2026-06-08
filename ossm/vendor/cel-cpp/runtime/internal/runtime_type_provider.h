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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_TYPE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_TYPE_PROVIDER_H_

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::runtime_internal {

class RuntimeTypeProvider final : public TypeReflector {
 public:
  explicit RuntimeTypeProvider(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool)
      : descriptor_pool_(descriptor_pool) {}

  absl::Status RegisterType(const OpaqueType& type);

  absl::StatusOr<absl_nullable ValueBuilderPtr> NewValueBuilder(
      absl::string_view name,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override;

 protected:
  absl::StatusOr<absl::optional<Type>> FindTypeImpl(
      absl::string_view name) const override;

  absl::StatusOr<absl::optional<EnumConstant>> FindEnumConstantImpl(
      absl::string_view type, absl::string_view value) const override;

  absl::StatusOr<absl::optional<StructTypeField>> FindStructTypeFieldByNameImpl(
      absl::string_view type, absl::string_view name) const override;

 private:
  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool_;
  absl::flat_hash_map<absl::string_view, Type> types_;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_RUNTIME_TYPE_PROVIDER_H_
