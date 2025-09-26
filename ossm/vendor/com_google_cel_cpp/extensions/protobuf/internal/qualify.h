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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_QUALIFY_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_QUALIFY_H_

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/attribute.h"
#include "common/memory.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"

namespace cel::extensions::protobuf_internal {

class ProtoQualifyState {
 public:
  ProtoQualifyState(const google::protobuf::Message* absl_nonnull message,
                    const google::protobuf::Descriptor* absl_nonnull descriptor,
                    const google::protobuf::Reflection* absl_nonnull reflection)
      : message_(message),
        descriptor_(descriptor),
        reflection_(reflection),
        repeated_field_desc_(nullptr) {}

  virtual ~ProtoQualifyState() = default;

  ProtoQualifyState(const ProtoQualifyState&) = delete;
  ProtoQualifyState& operator=(const ProtoQualifyState&) = delete;

  absl::Status ApplySelectQualifier(const cel::SelectQualifier& qualifier,
                                    MemoryManagerRef memory_manager);

  absl::Status ApplyLastQualifierHas(const cel::SelectQualifier& qualifier,
                                     MemoryManagerRef memory_manager);

  absl::Status ApplyLastQualifierGet(const cel::SelectQualifier& qualifier,
                                     MemoryManagerRef memory_manager);

 private:
  virtual void SetResultFromError(absl::Status status,
                                  MemoryManagerRef memory_manager) = 0;

  virtual void SetResultFromBool(bool value) = 0;

  virtual absl::Status SetResultFromField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      ProtoWrapperTypeOptions unboxing_option,
      MemoryManagerRef memory_manager) = 0;

  virtual absl::Status SetResultFromRepeatedField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      int index, MemoryManagerRef memory_manager) = 0;

  virtual absl::Status SetResultFromMapField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      const google::protobuf::MapValueConstRef& value,
      MemoryManagerRef memory_manager) = 0;

  absl::Status ApplyFieldSpecifier(const cel::FieldSpecifier& field_specifier,
                                   MemoryManagerRef memory_manager);

  absl::StatusOr<int> CheckListIndex(
      const cel::AttributeQualifier& qualifier) const;

  absl::Status ApplyAttributeQualifierList(
      const cel::AttributeQualifier& qualifier,
      MemoryManagerRef memory_manager);

  absl::StatusOr<google::protobuf::MapValueConstRef> CheckMapIndex(
      const cel::AttributeQualifier& qualifier) const;

  absl::Status ApplyAttributeQualifierMap(
      const cel::AttributeQualifier& qualifier,
      MemoryManagerRef memory_manager);

  absl::Status ApplyAttributeQualifer(const cel::AttributeQualifier& qualifier,
                                      MemoryManagerRef memory_manager);

  absl::Status MapHas(const cel::AttributeQualifier& key,
                      MemoryManagerRef memory_manager);

  absl::Status ApplyLastQualifierMessageGet(
      const cel::FieldSpecifier& specifier, MemoryManagerRef memory_manager);

  absl::Status ApplyLastQualifierGetList(
      const cel::AttributeQualifier& qualifier,
      MemoryManagerRef memory_manager);

  absl::Status ApplyLastQualifierGetMap(
      const cel::AttributeQualifier& qualifier,
      MemoryManagerRef memory_manager);

  const google::protobuf::Message* absl_nonnull message_;
  const google::protobuf::Descriptor* absl_nonnull descriptor_;
  const google::protobuf::Reflection* absl_nonnull reflection_;
  const google::protobuf::FieldDescriptor* absl_nullable repeated_field_desc_;
};

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_QUALIFY_H_
