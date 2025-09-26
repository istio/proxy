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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_MAP_REFLECTION_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_MAP_REFLECTION_H_

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

#ifndef GOOGLE_PROTOBUF_HAS_CEL_MAP_REFLECTION_FRIEND
#error "protobuf library is too old, please update to version 3.15.0 or newer"
#endif

namespace cel::extensions::protobuf_internal {

bool LookupMapValue(const google::protobuf::Reflection& reflection,
                    const google::protobuf::Message& message,
                    const google::protobuf::FieldDescriptor& field,
                    const google::protobuf::MapKey& key, google::protobuf::MapValueConstRef* value)
    ABSL_ATTRIBUTE_NONNULL();

bool ContainsMapKey(const google::protobuf::Reflection& reflection,
                    const google::protobuf::Message& message,
                    const google::protobuf::FieldDescriptor& field,
                    const google::protobuf::MapKey& key);

int MapSize(const google::protobuf::Reflection& reflection,
            const google::protobuf::Message& message,
            const google::protobuf::FieldDescriptor& field);

google::protobuf::MapIterator MapBegin(const google::protobuf::Reflection& reflection,
                             const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor& field);

google::protobuf::MapIterator MapEnd(const google::protobuf::Reflection& reflection,
                           const google::protobuf::Message& message,
                           const google::protobuf::FieldDescriptor& field);

bool InsertOrLookupMapValue(const google::protobuf::Reflection& reflection,
                            google::protobuf::Message* message,
                            const google::protobuf::FieldDescriptor& field,
                            const google::protobuf::MapKey& key,
                            google::protobuf::MapValueRef* value)
    ABSL_ATTRIBUTE_NONNULL();

bool DeleteMapValue(const google::protobuf::Reflection* absl_nonnull reflection,
                    google::protobuf::Message* absl_nonnull message,
                    const google::protobuf::FieldDescriptor* absl_nonnull field,
                    const google::protobuf::MapKey& key);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_MAP_REFLECTION_H_
