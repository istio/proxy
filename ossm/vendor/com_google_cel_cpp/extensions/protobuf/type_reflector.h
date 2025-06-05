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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_REFLECTOR_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_REFLECTOR_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "extensions/protobuf/type_introspector.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

class ProtoTypeReflector : public TypeReflector, public ProtoTypeIntrospector {
 public:
  ProtoTypeReflector()
      : ProtoTypeReflector(google::protobuf::DescriptorPool::generated_pool(),
                           google::protobuf::MessageFactory::generated_factory()) {}

  ProtoTypeReflector(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory)
      : ProtoTypeIntrospector(descriptor_pool),
        message_factory_(message_factory) {}

  absl::StatusOr<absl::Nullable<StructValueBuilderPtr>> NewStructValueBuilder(
      ValueFactory& value_factory, const StructType& type) const final;

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool()
      const override {
    return ProtoTypeIntrospector::descriptor_pool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() const override {
    return message_factory_;
  }

 private:
  absl::StatusOr<absl::optional<Value>> DeserializeValueImpl(
      ValueFactory& value_factory, absl::string_view type_url,
      const absl::Cord& value) const final;

  absl::Nonnull<google::protobuf::MessageFactory*> const message_factory_;
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_REFLECTOR_H_
