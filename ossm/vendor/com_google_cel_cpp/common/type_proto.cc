// Copyright 2025 Google LLC
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

#include "common/type_proto.h"

#include <cstddef>
#include <string>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

namespace {

using ::google::protobuf::NullValue;

using TypePb = cel::expr::Type;

// filter well-known types from message types.
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

absl::Status TypeToProtoInternal(const cel::Type& type,
                                 TypePb* absl_nonnull type_pb);

absl::Status ToProtoAbstractType(const cel::OpaqueType& type,
                                 TypePb* absl_nonnull type_pb) {
  auto* abstract_type = type_pb->mutable_abstract_type();
  abstract_type->set_name(type.name());
  abstract_type->mutable_parameter_types()->Reserve(
      type.GetParameters().size());

  for (const auto& param : type.GetParameters()) {
    CEL_RETURN_IF_ERROR(
        TypeToProtoInternal(param, abstract_type->add_parameter_types()));
  }

  return absl::OkStatus();
}

absl::Status ToProtoMapType(const cel::MapType& type,
                            TypePb* absl_nonnull type_pb) {
  auto* map_type = type_pb->mutable_map_type();
  CEL_RETURN_IF_ERROR(
      TypeToProtoInternal(type.key(), map_type->mutable_key_type()));
  CEL_RETURN_IF_ERROR(
      TypeToProtoInternal(type.value(), map_type->mutable_value_type()));

  return absl::OkStatus();
}

absl::Status ToProtoListType(const cel::ListType& type,
                             TypePb* absl_nonnull type_pb) {
  auto* list_type = type_pb->mutable_list_type();
  CEL_RETURN_IF_ERROR(
      TypeToProtoInternal(type.element(), list_type->mutable_elem_type()));

  return absl::OkStatus();
}

absl::Status ToProtoTypeType(const cel::TypeType& type,
                             TypePb* absl_nonnull type_pb) {
  if (type.GetParameters().size() > 1) {
    return absl::InternalError(
        absl::StrCat("unsupported type: ", type.DebugString()));
  }
  auto* type_type = type_pb->mutable_type();
  if (type.GetParameters().empty()) {
    return absl::OkStatus();
  }
  CEL_RETURN_IF_ERROR(TypeToProtoInternal(type.GetParameters()[0], type_type));
  return absl::OkStatus();
}

absl::Status TypeToProtoInternal(const cel::Type& type,
                                 TypePb* absl_nonnull type_pb) {
  switch (type.kind()) {
    case TypeKind::kDyn:
      type_pb->mutable_dyn();
      return absl::OkStatus();
    case TypeKind::kError:
      type_pb->mutable_error();
      return absl::OkStatus();
    case TypeKind::kNull:
      type_pb->set_null(NullValue::NULL_VALUE);
      return absl::OkStatus();
    case TypeKind::kBool:
      type_pb->set_primitive(TypePb::BOOL);
      return absl::OkStatus();
    case TypeKind::kInt:
      type_pb->set_primitive(TypePb::INT64);
      return absl::OkStatus();
    case TypeKind::kUint:
      type_pb->set_primitive(TypePb::UINT64);
      return absl::OkStatus();
    case TypeKind::kDouble:
      type_pb->set_primitive(TypePb::DOUBLE);
      return absl::OkStatus();
    case TypeKind::kString:
      type_pb->set_primitive(TypePb::STRING);
      return absl::OkStatus();
    case TypeKind::kBytes:
      type_pb->set_primitive(TypePb::BYTES);
      return absl::OkStatus();
    case TypeKind::kEnum:
      type_pb->set_primitive(TypePb::INT64);
      return absl::OkStatus();
    case TypeKind::kDuration:
      type_pb->set_well_known(TypePb::DURATION);
      return absl::OkStatus();
    case TypeKind::kTimestamp:
      type_pb->set_well_known(TypePb::TIMESTAMP);
      return absl::OkStatus();
    case TypeKind::kStruct:
      type_pb->set_message_type(type.GetStruct().name());
      return absl::OkStatus();
    case TypeKind::kList:
      return ToProtoListType(type.GetList(), type_pb);
    case TypeKind::kMap:
      return ToProtoMapType(type.GetMap(), type_pb);
    case TypeKind::kOpaque:
      return ToProtoAbstractType(type.GetOpaque(), type_pb);
    case TypeKind::kBoolWrapper:
      type_pb->set_wrapper(TypePb::BOOL);
      return absl::OkStatus();
    case TypeKind::kIntWrapper:
      type_pb->set_wrapper(TypePb::INT64);
      return absl::OkStatus();
    case TypeKind::kUintWrapper:
      type_pb->set_wrapper(TypePb::UINT64);
      return absl::OkStatus();
    case TypeKind::kDoubleWrapper:
      type_pb->set_wrapper(TypePb::DOUBLE);
      return absl::OkStatus();
    case TypeKind::kStringWrapper:
      type_pb->set_wrapper(TypePb::STRING);
      return absl::OkStatus();
    case TypeKind::kBytesWrapper:
      type_pb->set_wrapper(TypePb::BYTES);
      return absl::OkStatus();
    case TypeKind::kTypeParam:
      type_pb->set_type_param(type.GetTypeParam().name());
      return absl::OkStatus();
    case TypeKind::kType:
      return ToProtoTypeType(type.GetType(), type_pb);
    case TypeKind::kAny:
      type_pb->set_well_known(TypePb::ANY);
      return absl::OkStatus();
    default:
      return absl::InternalError(
          absl::StrCat("unsupported type: ", type.DebugString()));
  }
}

}  // namespace

absl::StatusOr<Type> TypeFromProto(
    const cel::expr::Type& type_pb,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena) {
  switch (type_pb.type_kind_case()) {
    case TypePb::kAbstractType: {
      auto* name = google::protobuf::Arena::Create<std::string>(
          arena, type_pb.abstract_type().name());
      std::vector<Type> params;
      params.resize(type_pb.abstract_type().parameter_types_size());
      size_t i = 0;
      for (const auto& p : type_pb.abstract_type().parameter_types()) {
        CEL_ASSIGN_OR_RETURN(params[i],
                             TypeFromProto(p, descriptor_pool, arena));
        i++;
      }
      return OpaqueType(arena, *name, params);
    }
    case TypePb::kDyn:
      return DynType();
    case TypePb::kError:
      return ErrorType();
    case TypePb::kListType: {
      CEL_ASSIGN_OR_RETURN(Type element,
                           TypeFromProto(type_pb.list_type().elem_type(),
                                         descriptor_pool, arena));
      return ListType(arena, element);
    }
    case TypePb::kMapType: {
      CEL_ASSIGN_OR_RETURN(
          Type key,
          TypeFromProto(type_pb.map_type().key_type(), descriptor_pool, arena));
      CEL_ASSIGN_OR_RETURN(Type value,
                           TypeFromProto(type_pb.map_type().value_type(),
                                         descriptor_pool, arena));
      return MapType(arena, key, value);
    }
    case TypePb::kMessageType: {
      if (auto well_known = MaybeWellKnownType(type_pb.message_type());
          well_known.has_value()) {
        return *well_known;
      }

      const auto* descriptor =
          descriptor_pool->FindMessageTypeByName(type_pb.message_type());
      if (descriptor == nullptr) {
        return absl::InvalidArgumentError(
            absl::StrCat("unknown message type: ", type_pb.message_type()));
      }
      return MessageType(descriptor);
    }
    case TypePb::kNull:
      return NullType();
    case TypePb::kPrimitive:
      switch (type_pb.primitive()) {
        case TypePb::BOOL:
          return BoolType();
        case TypePb::BYTES:
          return BytesType();
        case TypePb::DOUBLE:
          return DoubleType();
        case TypePb::INT64:
          return IntType();
        case TypePb::STRING:
          return StringType();
        case TypePb::UINT64:
          return UintType();
        default:
          return absl::InvalidArgumentError("unknown primitive kind");
      }
    case TypePb::kType: {
      CEL_ASSIGN_OR_RETURN(
          Type nested, TypeFromProto(type_pb.type(), descriptor_pool, arena));
      return TypeType(arena, nested);
    }
    case TypePb::kTypeParam: {
      auto* name =
          google::protobuf::Arena::Create<std::string>(arena, type_pb.type_param());
      return TypeParamType(*name);
    }
    case TypePb::kWellKnown:
      switch (type_pb.well_known()) {
        case TypePb::ANY:
          return AnyType();
        case TypePb::DURATION:
          return DurationType();
        case TypePb::TIMESTAMP:
          return TimestampType();
        default:
          break;
      }
      return absl::InvalidArgumentError("unknown well known type.");
    case TypePb::kWrapper: {
      switch (type_pb.wrapper()) {
        case TypePb::BOOL:
          return BoolWrapperType();
        case TypePb::BYTES:
          return BytesWrapperType();
        case TypePb::DOUBLE:
          return DoubleWrapperType();
        case TypePb::INT64:
          return IntWrapperType();
        case TypePb::STRING:
          return StringWrapperType();
        case TypePb::UINT64:
          return UintWrapperType();
        default:
          return absl::InvalidArgumentError("unknown primitive wrapper kind");
      }
    }
    // Function types are not supported in the C++ type checker.
    case TypePb::kFunction:
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unsupported type kind: ", type_pb.type_kind_case()));
  }
}

absl::Status TypeToProto(const Type& type, TypePb* absl_nonnull type_pb) {
  return TypeToProtoInternal(type, type_pb);
}

}  // namespace cel
