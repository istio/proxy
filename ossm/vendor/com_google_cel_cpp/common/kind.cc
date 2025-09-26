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

#include "common/kind.h"

#include "absl/strings/string_view.h"

namespace cel {

absl::string_view KindToString(Kind kind) {
  switch (kind) {
    case Kind::kNullType:
      return "null_type";
    case Kind::kDyn:
      return "dyn";
    case Kind::kAny:
      return "any";
    case Kind::kType:
      return "type";
    case Kind::kTypeParam:
      return "type_param";
    case Kind::kFunction:
      return "function";
    case Kind::kBool:
      return "bool";
    case Kind::kInt:
      return "int";
    case Kind::kUint:
      return "uint";
    case Kind::kDouble:
      return "double";
    case Kind::kString:
      return "string";
    case Kind::kBytes:
      return "bytes";
    case Kind::kDuration:
      return "duration";
    case Kind::kTimestamp:
      return "timestamp";
    case Kind::kList:
      return "list";
    case Kind::kMap:
      return "map";
    case Kind::kStruct:
      return "struct";
    case Kind::kUnknown:
      return "*unknown*";
    case Kind::kOpaque:
      return "*opaque*";
    case Kind::kBoolWrapper:
      return "google.protobuf.BoolValue";
    case Kind::kIntWrapper:
      return "google.protobuf.Int64Value";
    case Kind::kUintWrapper:
      return "google.protobuf.UInt64Value";
    case Kind::kDoubleWrapper:
      return "google.protobuf.DoubleValue";
    case Kind::kStringWrapper:
      return "google.protobuf.StringValue";
    case Kind::kBytesWrapper:
      return "google.protobuf.BytesValue";
    case Kind::kEnum:
      return "enum";
    default:
      return "*error*";
  }
}

}  // namespace cel
