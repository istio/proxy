// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "proto_field_extraction/field_extractor/field_extractor_util.h"

#include <functional>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "proto_field_extraction/utils/constants.h"

namespace google::protobuf::field_extraction {
namespace {

using ::google::protobuf::Field;
using ::google::protobuf::Type;

}  // namespace

// Proto map field is encoded as a repeated field of MapFieldEntry
// message on the wire.
//
// For map fields:
//     map<KeyType, ValueType> map_field = 1;
//
// The parsed descriptor looks like:
//     message MapFieldEntry {
//       option map_entry = true;
//       optional KeyType key = 1;
//       optional ValueType value = 2;
//     }
//     repeated MapFieldEntry map_field = 1;
//
// See MessageOptions.map_entry in
// http://google3/net/proto2/proto/descriptor.proto.
bool IsMapMessageType(const Type* type,
                      absl::string_view proto_map_entry_name) {
  if (type != nullptr) {
    for (const auto& option : type->options()) {
      if (option.name() == proto_map_entry_name) {
        return true;
      }
    }
  }
  return false;
}

bool IsAnyMessageType(const Type* type) {
  return type != nullptr && type->name() == kAnyType;
}

const Field* FindField(const Type& type, const absl::string_view field_name) {
  for (const auto& field : type.fields()) {
    if (field.name() == field_name) {
      return &field;
    }
  }
  return nullptr;
}

std::vector<absl::string_view> ConvertValuesToStrings(
    absl::Span<const google::protobuf::Value> values) {
  std::vector<absl::string_view> result;
  result.reserve(values.size());
  for (const auto& value : values) {
    if (value.has_string_value()) {
      result.push_back(value.string_value());
    } else if (value.has_struct_value()) {
      for (const auto& field : value.struct_value().fields()) {
        if (field.second.has_string_value()) {
          result.push_back(field.second.string_value());
        }
      }
    }
  }

  return result;
}

}  // namespace google::protobuf::field_extraction
