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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_INTERNAL_FIELD_BACKED_MAP_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_INTERNAL_FIELD_BACKED_MAP_IMPL_H_

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/protobuf_value_factory.h"

namespace google::api::expr::runtime::internal {
// CelMap implementation that uses "map" message field
// as backing storage.
class FieldBackedMapImpl : public CelMap {
 public:
  // message contains the "map" field. Object stores the pointer
  // to the message, thus it is expected that message outlives the
  // object.
  // descriptor FieldDescriptor for the field
  FieldBackedMapImpl(const google::protobuf::Message* message,
                     const google::protobuf::FieldDescriptor* descriptor,
                     ProtobufValueFactory factory, google::protobuf::Arena* arena);

  // Map size.
  int size() const override;

  // Map element access operator.
  absl::optional<CelValue> operator[](CelValue key) const override;

  // Presence test function.
  absl::StatusOr<bool> Has(const CelValue& key) const override;

  absl::StatusOr<const CelList*> ListKeys() const override;

 protected:
  // These methods are exposed as protected methods for testing purposes since
  // whether one or the other is used depends on build time flags, but each
  // should be tested accordingly.

  absl::StatusOr<bool> LookupMapValue(
      const CelValue& key, google::protobuf::MapValueConstRef* value_ref) const;

  absl::StatusOr<bool> LegacyHasMapValue(const CelValue& key) const;

  absl::optional<CelValue> LegacyLookupMapValue(const CelValue& key) const;

 private:
  const google::protobuf::Message* message_;
  const google::protobuf::FieldDescriptor* descriptor_;
  const google::protobuf::FieldDescriptor* key_desc_;
  const google::protobuf::FieldDescriptor* value_desc_;
  const google::protobuf::Reflection* reflection_;
  ProtobufValueFactory factory_;
  google::protobuf::Arena* arena_;
  std::unique_ptr<CelList> key_list_;
};

}  // namespace google::api::expr::runtime::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_INTERNAL_FIELD_BACKED_MAP_IMPL_H_
