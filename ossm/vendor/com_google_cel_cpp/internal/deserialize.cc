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

#include "internal/deserialize.h"

#include <cstdint>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel::internal {

absl::StatusOr<absl::Duration> DeserializeDuration(const absl::Cord& data) {
  int64_t seconds = 0;
  int32_t nanos = 0;
  ProtoWireDecoder decoder("google.protobuf.Duration", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(seconds, decoder.ReadVarint<int64_t>());
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(nanos, decoder.ReadVarint<int32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Time> DeserializeTimestamp(const absl::Cord& data) {
  int64_t seconds = 0;
  int32_t nanos = 0;
  ProtoWireDecoder decoder("google.protobuf.Timestamp", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(seconds, decoder.ReadVarint<int64_t>());
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(nanos, decoder.ReadVarint<int32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return absl::UnixEpoch() + absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Cord> DeserializeBytesValue(const absl::Cord& data) {
  absl::Cord primitive;
  ProtoWireDecoder decoder("google.protobuf.BytesValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadLengthDelimited());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<absl::Cord> DeserializeStringValue(const absl::Cord& data) {
  absl::Cord primitive;
  ProtoWireDecoder decoder("google.protobuf.StringValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadLengthDelimited());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<bool> DeserializeBoolValue(const absl::Cord& data) {
  bool primitive = false;
  ProtoWireDecoder decoder("google.protobuf.BoolValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<bool>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<int32_t> DeserializeInt32Value(const absl::Cord& data) {
  int32_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.Int32Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<int32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<int64_t> DeserializeInt64Value(const absl::Cord& data) {
  int64_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.Int64Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<int64_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<uint32_t> DeserializeUInt32Value(const absl::Cord& data) {
  uint32_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.UInt32Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<uint32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<uint64_t> DeserializeUInt64Value(const absl::Cord& data) {
  uint64_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.UInt64Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<uint64_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<float> DeserializeFloatValue(const absl::Cord& data) {
  float primitive = 0.0f;
  ProtoWireDecoder decoder("google.protobuf.FloatValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed32)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed32<float>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<double> DeserializeDoubleValue(const absl::Cord& data) {
  double primitive = 0.0;
  ProtoWireDecoder decoder("google.protobuf.DoubleValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed64)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed64<double>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<double> DeserializeFloatValueOrDoubleValue(
    const absl::Cord& data) {
  double primitive = 0.0;
  ProtoWireDecoder decoder("google.protobuf.DoubleValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed32)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed32<float>());
      continue;
    }
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed64)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed64<double>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<Json> DeserializeValue(const absl::Cord& data) {
  Json json = kJsonNull;
  ProtoWireDecoder decoder("google.protobuf.Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(auto unused, decoder.ReadVarint<bool>());
      static_cast<void>(unused);
      json = kJsonNull;
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kFixed64)) {
      CEL_ASSIGN_OR_RETURN(auto number_value, decoder.ReadFixed64<double>());
      json = number_value;
      continue;
    }
    if (tag == MakeProtoWireTag(3, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto string_value, decoder.ReadLengthDelimited());
      json = std::move(string_value);
      continue;
    }
    if (tag == MakeProtoWireTag(4, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(auto bool_value, decoder.ReadVarint<bool>());
      json = bool_value;
      continue;
    }
    if (tag == MakeProtoWireTag(5, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto struct_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(auto json_object, DeserializeStruct(struct_value));
      json = std::move(json_object);
      continue;
    }
    if (tag == MakeProtoWireTag(6, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(auto list_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(auto json_array, DeserializeListValue(list_value));
      json = std::move(json_array);
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return json;
}

absl::StatusOr<JsonArray> DeserializeListValue(const absl::Cord& data) {
  JsonArrayBuilder array_builder;
  ProtoWireDecoder decoder("google.protobuf.ListValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      // values
      CEL_ASSIGN_OR_RETURN(auto element_value, decoder.ReadLengthDelimited());
      CEL_ASSIGN_OR_RETURN(auto element, DeserializeValue(element_value));
      array_builder.push_back(std::move(element));
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return std::move(array_builder).Build();
}

absl::StatusOr<JsonObject> DeserializeStruct(const absl::Cord& data) {
  JsonObjectBuilder object_builder;
  ProtoWireDecoder decoder("google.protobuf.Struct", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      // fields
      CEL_ASSIGN_OR_RETURN(auto fields_value, decoder.ReadLengthDelimited());
      absl::Cord field_name;
      Json field_value = kJsonNull;
      ProtoWireDecoder fields_decoder("google.protobuf.Struct.FieldsEntry",
                                      fields_value);
      while (fields_decoder.HasNext()) {
        CEL_ASSIGN_OR_RETURN(auto fields_tag, fields_decoder.ReadTag());
        if (fields_tag ==
            MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
          // key
          CEL_ASSIGN_OR_RETURN(field_name,
                               fields_decoder.ReadLengthDelimited());
          continue;
        }
        if (fields_tag ==
            MakeProtoWireTag(2, ProtoWireType::kLengthDelimited)) {
          // value
          CEL_ASSIGN_OR_RETURN(auto field_value_value,
                               fields_decoder.ReadLengthDelimited());
          CEL_ASSIGN_OR_RETURN(field_value,
                               DeserializeValue(field_value_value));
          continue;
        }
        CEL_RETURN_IF_ERROR(fields_decoder.SkipLengthValue());
      }
      fields_decoder.EnsureFullyDecoded();
      object_builder.insert_or_assign(std::move(field_name),
                                      std::move(field_value));
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return std::move(object_builder).Build();
}

absl::StatusOr<google::protobuf::Any> DeserializeAny(const absl::Cord& data) {
  absl::Cord type_url;
  absl::Cord value;
  ProtoWireDecoder decoder("google.protobuf.Any", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(type_url, decoder.ReadLengthDelimited());
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(value, decoder.ReadLengthDelimited());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return MakeAny(static_cast<std::string>(type_url), std::move(value));
}

}  // namespace cel::internal
