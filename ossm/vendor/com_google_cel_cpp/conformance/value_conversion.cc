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
#include "conformance/value_conversion.h"

#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/any.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "extensions/protobuf/value.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::conformance_internal {
namespace {

using ConformanceKind = google::api::expr::v1alpha1::Value::KindCase;
using ConformanceMapValue = google::api::expr::v1alpha1::MapValue;
using ConformanceListValue = google::api::expr::v1alpha1::ListValue;

std::string ToString(ConformanceKind kind_case) {
  switch (kind_case) {
    case ConformanceKind::kBoolValue:
      return "bool_value";
    case ConformanceKind::kInt64Value:
      return "int64_value";
    case ConformanceKind::kUint64Value:
      return "uint64_value";
    case ConformanceKind::kDoubleValue:
      return "double_value";
    case ConformanceKind::kStringValue:
      return "string_value";
    case ConformanceKind::kBytesValue:
      return "bytes_value";
    case ConformanceKind::kTypeValue:
      return "type_value";
    case ConformanceKind::kEnumValue:
      return "enum_value";
    case ConformanceKind::kMapValue:
      return "map_value";
    case ConformanceKind::kListValue:
      return "list_value";
    case ConformanceKind::kNullValue:
      return "null_value";
    case ConformanceKind::kObjectValue:
      return "object_value";
    default:
      return "unknown kind case";
  }
}

absl::StatusOr<Value> FromObject(ValueManager& value_manager,
                                 const google::protobuf::Any& any) {
  if (any.type_url() == "type.googleapis.com/google.protobuf.Duration") {
    google::protobuf::Duration duration;
    if (!any.UnpackTo(&duration)) {
      return absl::InvalidArgumentError("invalid duration");
    }
    return value_manager.CreateDurationValue(
        internal::DecodeDuration(duration));
  } else if (any.type_url() ==
             "type.googleapis.com/google.protobuf.Timestamp") {
    google::protobuf::Timestamp timestamp;
    if (!any.UnpackTo(&timestamp)) {
      return absl::InvalidArgumentError("invalid timestamp");
    }
    return value_manager.CreateTimestampValue(internal::DecodeTime(timestamp));
  }

  return extensions::ProtoMessageToValue(value_manager, any);
}

absl::StatusOr<MapValue> MapValueFromConformance(
    ValueManager& value_manager, const ConformanceMapValue& map_value) {
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewMapValueBuilder(MapType{}));
  for (const auto& entry : map_value.entries()) {
    CEL_ASSIGN_OR_RETURN(auto key,
                         FromConformanceValue(value_manager, entry.key()));
    CEL_ASSIGN_OR_RETURN(auto value,
                         FromConformanceValue(value_manager, entry.value()));
    CEL_RETURN_IF_ERROR(builder->Put(std::move(key), std::move(value)));
  }

  return std::move(*builder).Build();
}

absl::StatusOr<ListValue> ListValueFromConformance(
    ValueManager& value_manager, const ConformanceListValue& list_value) {
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType{}));
  for (const auto& elem : list_value.values()) {
    CEL_ASSIGN_OR_RETURN(auto value, FromConformanceValue(value_manager, elem));
    CEL_RETURN_IF_ERROR(builder->Add(std::move(value)));
  }

  return std::move(*builder).Build();
}

absl::StatusOr<ConformanceMapValue> MapValueToConformance(
    ValueManager& value_manager, const MapValue& map_value) {
  ConformanceMapValue result;

  CEL_ASSIGN_OR_RETURN(auto iter, map_value.NewIterator(value_manager));

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto key_value, iter->Next(value_manager));
    CEL_ASSIGN_OR_RETURN(auto value_value,
                         map_value.Get(value_manager, key_value));

    CEL_ASSIGN_OR_RETURN(auto key,
                         ToConformanceValue(value_manager, key_value));
    CEL_ASSIGN_OR_RETURN(auto value,
                         ToConformanceValue(value_manager, value_value));

    auto* entry = result.add_entries();

    *entry->mutable_key() = std::move(key);
    *entry->mutable_value() = std::move(value);
  }

  return result;
}

absl::StatusOr<ConformanceListValue> ListValueToConformance(
    ValueManager& value_manager, const ListValue& list_value) {
  ConformanceListValue result;

  CEL_ASSIGN_OR_RETURN(auto iter, list_value.NewIterator(value_manager));

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto elem, iter->Next(value_manager));
    CEL_ASSIGN_OR_RETURN(*result.add_values(),
                         ToConformanceValue(value_manager, elem));
  }

  return result;
}

absl::StatusOr<google::protobuf::Any> ToProtobufAny(
    ValueManager& value_manager, const StructValue& struct_value) {
  absl::Cord serialized;
  CEL_RETURN_IF_ERROR(struct_value.SerializeTo(value_manager, serialized));
  google::protobuf::Any result;
  result.set_type_url(MakeTypeUrl(struct_value.GetTypeName()));
  result.set_value(std::string(serialized));

  return result;
}

// filter well-known types from MessageTypes provided to conformance.
absl::optional<Type> MaybeWellKnownType(absl::string_view type_name) {
  static const absl::flat_hash_map<absl::string_view, Type>* kWellKnownTypes =
      []() {
        auto* instance = new absl::flat_hash_map<absl::string_view, Type>{
            // keep-sorted start
            {"google.protobuf.Any", AnyType()},
            {"google.protobuf.BoolValue", BoolWrapperType()},
            {"google.protobuf.BytesValue", BytesWrapperType()},
            {"google.protobuf.DoubleValue", DoubleWrapperType()},
            {"google.protobuf.Duration", DurationType()},
            {"google.protobuf.FloatValue", DoubleWrapperType()},
            {"google.protobuf.Int32Value", IntWrapperType()},
            {"google.protobuf.Int64Value", IntWrapperType()},
            {"google.protobuf.ListValue", ListType()},
            {"google.protobuf.StringValue", StringWrapperType()},
            {"google.protobuf.Struct", JsonMapType()},
            {"google.protobuf.Timestamp", TimestampType()},
            {"google.protobuf.UInt32Value", UintWrapperType()},
            {"google.protobuf.UInt64Value", UintWrapperType()},
            {"google.protobuf.Value", DynType()},
            // keep-sorted end
        };
        return instance;
      }();

  if (auto it = kWellKnownTypes->find(type_name);
      it != kWellKnownTypes->end()) {
    return it->second;
  }

  return absl::nullopt;
}

}  // namespace

absl::StatusOr<Value> FromConformanceValue(
    ValueManager& value_manager, const google::api::expr::v1alpha1::Value& value) {
  google::protobuf::LinkMessageReflection<google::api::expr::v1alpha1::Value>();
  switch (value.kind_case()) {
    case ConformanceKind::kBoolValue:
      return value_manager.CreateBoolValue(value.bool_value());
    case ConformanceKind::kInt64Value:
      return value_manager.CreateIntValue(value.int64_value());
    case ConformanceKind::kUint64Value:
      return value_manager.CreateUintValue(value.uint64_value());
    case ConformanceKind::kDoubleValue:
      return value_manager.CreateDoubleValue(value.double_value());
    case ConformanceKind::kStringValue:
      return value_manager.CreateStringValue(value.string_value());
    case ConformanceKind::kBytesValue:
      return value_manager.CreateBytesValue(value.bytes_value());
    case ConformanceKind::kNullValue:
      return value_manager.GetNullValue();
    case ConformanceKind::kObjectValue:
      return FromObject(value_manager, value.object_value());
    case ConformanceKind::kMapValue:
      return MapValueFromConformance(value_manager, value.map_value());
    case ConformanceKind::kListValue:
      return ListValueFromConformance(value_manager, value.list_value());

    default:
      return absl::UnimplementedError(absl::StrCat(
          "FromConformanceValue not supported ", ToString(value.kind_case())));
  }
}

absl::StatusOr<google::api::expr::v1alpha1::Value> ToConformanceValue(
    ValueManager& value_manager, const Value& value) {
  google::api::expr::v1alpha1::Value result;
  switch (value->kind()) {
    case ValueKind::kBool:
      result.set_bool_value(value.GetBool().NativeValue());
      break;
    case ValueKind::kInt:
      result.set_int64_value(value.GetInt().NativeValue());
      break;
    case ValueKind::kUint:
      result.set_uint64_value(value.GetUint().NativeValue());
      break;
    case ValueKind::kDouble:
      result.set_double_value(value.GetDouble().NativeValue());
      break;
    case ValueKind::kString:
      result.set_string_value(value.GetString().ToString());
      break;
    case ValueKind::kBytes:
      result.set_bytes_value(value.GetBytes().ToString());
      break;
    case ValueKind::kType:
      result.set_type_value(value.GetType().name());
      break;
    case ValueKind::kNull:
      result.set_null_value(google::protobuf::NullValue::NULL_VALUE);
      break;
    case ValueKind::kDuration: {
      google::protobuf::Duration duration;
      CEL_RETURN_IF_ERROR(internal::EncodeDuration(
          value.GetDuration().NativeValue(), &duration));
      result.mutable_object_value()->PackFrom(duration);
      break;
    }
    case ValueKind::kTimestamp: {
      google::protobuf::Timestamp timestamp;
      CEL_RETURN_IF_ERROR(
          internal::EncodeTime(value.GetTimestamp().NativeValue(), &timestamp));
      result.mutable_object_value()->PackFrom(timestamp);
      break;
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(
          *result.mutable_map_value(),
          MapValueToConformance(value_manager, value.GetMap()));
      break;
    }
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(
          *result.mutable_list_value(),
          ListValueToConformance(value_manager, value.GetList()));
      break;
    }
    case ValueKind::kStruct: {
      CEL_ASSIGN_OR_RETURN(*result.mutable_object_value(),
                           ToProtobufAny(value_manager, value.GetStruct()));
      break;
    }
    default:
      return absl::UnimplementedError(
          absl::StrCat("ToConformanceValue not supported ",
                       ValueKindToString(value->kind())));
  }
  return result;
}

absl::StatusOr<Type> FromConformanceType(google::protobuf::Arena* arena,
                                         const google::api::expr::v1alpha1::Type& type) {
  switch (type.type_kind_case()) {
    case google::api::expr::v1alpha1::Type::kNull:
      return NullType();
    case google::api::expr::v1alpha1::Type::kDyn:
      return DynType();
    case google::api::expr::v1alpha1::Type::kPrimitive: {
      switch (type.primitive()) {
        case google::api::expr::v1alpha1::Type::BOOL:
          return BoolType();
        case google::api::expr::v1alpha1::Type::INT64:
          return IntType();
        case google::api::expr::v1alpha1::Type::UINT64:
          return UintType();
        case google::api::expr::v1alpha1::Type::DOUBLE:
          return DoubleType();
        case google::api::expr::v1alpha1::Type::STRING:
          return StringType();
        case google::api::expr::v1alpha1::Type::BYTES:
          return BytesType();
        default:
          return absl::UnimplementedError(absl::StrCat(
              "FromConformanceType not supported ", type.primitive()));
      }
    }
    case google::api::expr::v1alpha1::Type::kWrapper: {
      switch (type.wrapper()) {
        case google::api::expr::v1alpha1::Type::BOOL:
          return BoolWrapperType();
        case google::api::expr::v1alpha1::Type::INT64:
          return IntWrapperType();
        case google::api::expr::v1alpha1::Type::UINT64:
          return UintWrapperType();
        case google::api::expr::v1alpha1::Type::DOUBLE:
          return DoubleWrapperType();
        case google::api::expr::v1alpha1::Type::STRING:
          return StringWrapperType();
        case google::api::expr::v1alpha1::Type::BYTES:
          return BytesWrapperType();
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "FromConformanceType not supported ", type.wrapper()));
      }
    }
    case google::api::expr::v1alpha1::Type::kWellKnown: {
      switch (type.well_known()) {
        case google::api::expr::v1alpha1::Type::DURATION:
          return DurationType();
        case google::api::expr::v1alpha1::Type::TIMESTAMP:
          return TimestampType();
        case google::api::expr::v1alpha1::Type::ANY:
          return DynType();
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "FromConformanceType not supported ", type.well_known()));
      }
    }
    case google::api::expr::v1alpha1::Type::kListType: {
      CEL_ASSIGN_OR_RETURN(
          Type element_type,
          FromConformanceType(arena, type.list_type().elem_type()));
      return ListType(arena, element_type);
    }
    case google::api::expr::v1alpha1::Type::kMapType: {
      CEL_ASSIGN_OR_RETURN(
          auto key_type,
          FromConformanceType(arena, type.map_type().key_type()));
      CEL_ASSIGN_OR_RETURN(
          auto value_type,
          FromConformanceType(arena, type.map_type().value_type()));
      return MapType(arena, key_type, value_type);
    }
    case google::api::expr::v1alpha1::Type::kFunction: {
      return absl::UnimplementedError("Function support not yet implemented");
    }
    case google::api::expr::v1alpha1::Type::kMessageType: {
      if (absl::optional<Type> wkt = MaybeWellKnownType(type.message_type());
          wkt.has_value()) {
        return *wkt;
      }
      const google::protobuf::Descriptor* descriptor =
          google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
              type.message_type());
      if (descriptor == nullptr) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Message type: '", type.message_type(), "' not linked."));
      }
      return MessageType(descriptor);
    }
    case google::api::expr::v1alpha1::Type::kTypeParam: {
      auto* param =
          google::protobuf::Arena::Create<std::string>(arena, type.type_param());
      return TypeParamType(*param);
    }
    case google::api::expr::v1alpha1::Type::kType: {
      CEL_ASSIGN_OR_RETURN(Type param_type,
                           FromConformanceType(arena, type.type()));
      return TypeType(arena, param_type);
    }
    case google::api::expr::v1alpha1::Type::kError: {
      return absl::InvalidArgumentError("Error type not supported");
    }
    case google::api::expr::v1alpha1::Type::kAbstractType: {
      std::vector<Type> parameters;
      for (const auto& param : type.abstract_type().parameter_types()) {
        CEL_ASSIGN_OR_RETURN(auto param_type,
                             FromConformanceType(arena, param));
        parameters.push_back(std::move(param_type));
      }
      return OpaqueType(arena, type.abstract_type().name(), parameters);
    }
    default:
      return absl::UnimplementedError(absl::StrCat(
          "FromConformanceType not supported ", type.type_kind_case()));
  }
  return absl::InternalError("FromConformanceType not supported: fallthrough");
}

}  // namespace cel::conformance_internal
