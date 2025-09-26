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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_ADAPTER_H_

#include "absl/status/status.h"
#include "runtime/type_registry.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

// Register a resolveable enum for the given runtime builder.
absl::Status RegisterProtobufEnum(
    TypeRegistry& registry, const google::protobuf::EnumDescriptor* enum_descriptor);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_ADAPTER_H_
