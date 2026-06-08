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
#include "common/type_reflector.h"
#include "extensions/protobuf/type_introspector.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

class ProtoTypeReflector : public TypeReflector, public ProtoTypeIntrospector {
 public:
  ProtoTypeReflector()
      : ProtoTypeReflector(google::protobuf::DescriptorPool::generated_pool()) {}

  explicit ProtoTypeReflector(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool)
      : ProtoTypeIntrospector(descriptor_pool) {}

  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool() const {
    return ProtoTypeIntrospector::descriptor_pool();
  }
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_REFLECTOR_H_
