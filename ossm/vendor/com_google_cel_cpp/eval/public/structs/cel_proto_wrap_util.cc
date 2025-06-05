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

#include "eval/public/structs/cel_proto_wrap_util.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/protobuf_value_factory.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/overflow.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace google::api::expr::runtime::internal {

namespace {

using cel::internal::DecodeDuration;
using cel::internal::DecodeTime;
using google::protobuf::Any;
using google::protobuf::BoolValue;
using google::protobuf::BytesValue;
using google::protobuf::DoubleValue;
using google::protobuf::Duration;
using google::protobuf::FloatValue;
using google::protobuf::Int32Value;
using google::protobuf::Int64Value;
using google::protobuf::ListValue;
using google::protobuf::StringValue;
using google::protobuf::Struct;
using google::protobuf::Timestamp;
using google::protobuf::UInt32Value;
using google::protobuf::UInt64Value;
using google::protobuf::Value;
using google::protobuf::Arena;
using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::Message;
using google::protobuf::MessageFactory;

// kMaxIntJSON is defined as the Number.MAX_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMaxIntJSON = (1ll << 53) - 1;

// kMinIntJSON is defined as the Number.MIN_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMinIntJSON = -kMaxIntJSON;

// IsJSONSafe indicates whether the int is safely representable as a floating
// point value in JSON.
static bool IsJSONSafe(int64_t i) {
  return i >= kMinIntJSON && i <= kMaxIntJSON;
}

// IsJSONSafe indicates whether the uint is safely representable as a floating
// point value in JSON.
static bool IsJSONSafe(uint64_t i) {
  return i <= static_cast<uint64_t>(kMaxIntJSON);
}

// Map implementation wrapping google.protobuf.ListValue
class DynamicList : public CelList {
 public:
  DynamicList(const ListValue* values, ProtobufValueFactory factory,
              Arena* arena)
      : arena_(arena), factory_(std::move(factory)), values_(values) {}

  CelValue operator[](int index) const override;

  // List size
  int size() const override { return values_->values_size(); }

 private:
  Arena* arena_;
  ProtobufValueFactory factory_;
  const ListValue* values_;
};

// Map implementation wrapping google.protobuf.Struct.
class DynamicMap : public CelMap {
 public:
  DynamicMap(const Struct* values, ProtobufValueFactory factory, Arena* arena)
      : arena_(arena),
        factory_(std::move(factory)),
        values_(values),
        key_list_(values) {}

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    CelValue::StringHolder str_key;
    if (!key.GetValue(&str_key)) {
      // Not a string key.
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", CelValue::TypeName(key.type()), "'"));
    }

    return values_->fields().contains(std::string(str_key.value()));
  }

  absl::optional<CelValue> operator[](CelValue key) const override;

  int size() const override { return values_->fields_size(); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return &key_list_;
  }

 private:
  // List of keys in Struct.fields map.
  // It utilizes lazy initialization, to avoid performance penalties.
  class DynamicMapKeyList : public CelList {
   public:
    explicit DynamicMapKeyList(const Struct* values)
        : values_(values), keys_(), initialized_(false) {}

    // Index access
    CelValue operator[](int index) const override {
      CheckInit();
      return keys_[index];
    }

    // List size
    int size() const override {
      CheckInit();
      return values_->fields_size();
    }

   private:
    void CheckInit() const {
      absl::MutexLock lock(&mutex_);
      if (!initialized_) {
        for (const auto& it : values_->fields()) {
          keys_.push_back(CelValue::CreateString(&it.first));
        }
        initialized_ = true;
      }
    }

    const Struct* values_;
    mutable absl::Mutex mutex_;
    mutable std::vector<CelValue> keys_;
    mutable bool initialized_;
  };

  Arena* arena_;
  ProtobufValueFactory factory_;
  const Struct* values_;
  const DynamicMapKeyList key_list_;
};

// Adapter for usage with CEL_RETURN_IF_ERROR and CEL_ASSIGN_OR_RETURN.
class ReturnCelValueError {
 public:
  explicit ReturnCelValueError(absl::Nonnull<google::protobuf::Arena*> arena)
      : arena_(arena) {}

  CelValue operator()(const absl::Status& status) const {
    ABSL_DCHECK(!status.ok());
    return CelValue::CreateError(
        google::protobuf::Arena::Create<absl::Status>(arena_, status));
  }

 private:
  absl::Nonnull<google::protobuf::Arena*> arena_;
};

struct IgnoreErrorAndReturnNullptr {
  std::nullptr_t operator()(const absl::Status& status) const {
    status.IgnoreError();
    return nullptr;
  }
};

// ValueManager provides ValueFromMessage(....) function family.
// Functions of this family create CelValue object from specific subtypes of
// protobuf message.
class ValueManager {
 public:
  ValueManager(const ProtobufValueFactory& value_factory,
               const google::protobuf::DescriptorPool* descriptor_pool,
               google::protobuf::Arena* arena, google::protobuf::MessageFactory* message_factory)
      : value_factory_(value_factory),
        descriptor_pool_(descriptor_pool),
        arena_(arena),
        message_factory_(message_factory) {}

  // Note: this overload should only be used in the context of accessing struct
  // value members, which have already been adapted to the generated message
  // types.
  ValueManager(const ProtobufValueFactory& value_factory, google::protobuf::Arena* arena)
      : value_factory_(value_factory),
        descriptor_pool_(DescriptorPool::generated_pool()),
        arena_(arena),
        message_factory_(MessageFactory::generated_factory()) {}

  static CelValue ValueFromDuration(absl::Duration duration) {
    return CelValue::CreateDuration(duration);
  }

  CelValue ValueFromDuration(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(
        auto reflection,
        cel::well_known_types::GetDurationReflection(message->GetDescriptor()),
        _.With(ReturnCelValueError(arena_)));
    return ValueFromDuration(reflection.UnsafeToAbslDuration(*message));
  }

  CelValue ValueFromMessage(const Duration* duration) {
    return ValueFromDuration(DecodeDuration(*duration));
  }

  CelValue ValueFromTimestamp(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(
        auto reflection,
        cel::well_known_types::GetTimestampReflection(message->GetDescriptor()),
        _.With(ReturnCelValueError(arena_)));
    return ValueFromTimestamp(reflection.UnsafeToAbslTime(*message));
  }

  static CelValue ValueFromTimestamp(absl::Time timestamp) {
    return CelValue::CreateTimestamp(timestamp);
  }

  CelValue ValueFromMessage(const Timestamp* timestamp) {
    return ValueFromTimestamp(DecodeTime(*timestamp));
  }

  CelValue ValueFromMessage(const ListValue* list_values) {
    return CelValue::CreateList(Arena::Create<DynamicList>(
        arena_, list_values, value_factory_, arena_));
  }

  CelValue ValueFromMessage(const Struct* struct_value) {
    return CelValue::CreateMap(Arena::Create<DynamicMap>(
        arena_, struct_value, value_factory_, arena_));
  }

  CelValue ValueFromAny(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(
        auto reflection,
        cel::well_known_types::GetAnyReflection(message->GetDescriptor()),
        _.With(ReturnCelValueError(arena_)));
    std::string type_url_scratch;
    std::string value_scratch;
    return ValueFromAny(reflection.GetTypeUrl(*message, type_url_scratch),
                        reflection.GetValue(*message, value_scratch),
                        descriptor_pool_, message_factory_);
  }

  CelValue ValueFromAny(const cel::well_known_types::StringValue& type_url,
                        const cel::well_known_types::BytesValue& payload,
                        const DescriptorPool* descriptor_pool,
                        MessageFactory* message_factory) {
    std::string type_url_string_scratch;
    absl::string_view type_url_string = absl::visit(
        absl::Overload([](absl::string_view string)
                           -> absl::string_view { return string; },
                       [&type_url_string_scratch](
                           const absl::Cord& cord) -> absl::string_view {
                         if (auto flat = cord.TryFlat(); flat) {
                           return *flat;
                         }
                         absl::CopyCordToString(cord, &type_url_string_scratch);
                         return absl::string_view(type_url_string_scratch);
                       }),
        cel::well_known_types::AsVariant(type_url));
    auto pos = type_url_string.find_last_of('/');
    if (pos == type_url_string.npos) {
      // TODO What error code?
      // Malformed type_url
      return CreateErrorValue(arena_, "Malformed type_url string");
    }

    absl::string_view full_name = type_url_string.substr(pos + 1);
    const Descriptor* nested_descriptor =
        descriptor_pool->FindMessageTypeByName(full_name);

    if (nested_descriptor == nullptr) {
      // Descriptor not found for the type
      // TODO What error code?
      return CreateErrorValue(arena_, "Descriptor not found");
    }

    const Message* prototype = message_factory->GetPrototype(nested_descriptor);
    if (prototype == nullptr) {
      // Failed to obtain prototype for the descriptor
      // TODO What error code?
      return CreateErrorValue(arena_, "Prototype not found");
    }

    Message* nested_message = prototype->New(arena_);
    bool ok =
        absl::visit(absl::Overload(
                        [nested_message](absl::string_view string) -> bool {
                          return nested_message->ParsePartialFromString(string);
                        },
                        [nested_message](const absl::Cord& cord) -> bool {
                          return nested_message->ParsePartialFromCord(cord);
                        }),
                    cel::well_known_types::AsVariant(payload));
    if (!ok) {
      // Failed to unpack.
      // TODO What error code?
      return CreateErrorValue(arena_, "Failed to unpack Any into message");
    }

    return UnwrapMessageToValue(nested_message, value_factory_, arena_);
  }

  CelValue ValueFromMessage(const Any* any_value,
                            const DescriptorPool* descriptor_pool,
                            MessageFactory* message_factory) {
    return ValueFromAny(any_value->type_url(), absl::Cord(any_value->value()),
                        descriptor_pool, message_factory);
  }

  CelValue ValueFromMessage(const Any* any_value) {
    return ValueFromMessage(any_value, descriptor_pool_, message_factory_);
  }

  CelValue ValueFromBool(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(
        auto reflection,
        cel::well_known_types::GetBoolValueReflection(message->GetDescriptor()),
        _.With(ReturnCelValueError(arena_)));
    return ValueFromBool(reflection.GetValue(*message));
  }

  static CelValue ValueFromBool(bool value) {
    return CelValue::CreateBool(value);
  }

  CelValue ValueFromMessage(const BoolValue* wrapper) {
    return ValueFromBool(wrapper->value());
  }

  CelValue ValueFromInt32(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetInt32ValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    return ValueFromInt32(reflection.GetValue(*message));
  }

  static CelValue ValueFromInt32(int32_t value) {
    return CelValue::CreateInt64(value);
  }

  CelValue ValueFromMessage(const Int32Value* wrapper) {
    return ValueFromInt32(wrapper->value());
  }

  CelValue ValueFromUInt32(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetUInt32ValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    return ValueFromUInt32(reflection.GetValue(*message));
  }

  static CelValue ValueFromUInt32(uint32_t value) {
    return CelValue::CreateUint64(value);
  }

  CelValue ValueFromMessage(const UInt32Value* wrapper) {
    return ValueFromUInt32(wrapper->value());
  }

  CelValue ValueFromInt64(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetInt64ValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    return ValueFromInt64(reflection.GetValue(*message));
  }

  static CelValue ValueFromInt64(int64_t value) {
    return CelValue::CreateInt64(value);
  }

  CelValue ValueFromMessage(const Int64Value* wrapper) {
    return ValueFromInt64(wrapper->value());
  }

  CelValue ValueFromUInt64(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetUInt64ValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    return ValueFromUInt64(reflection.GetValue(*message));
  }

  static CelValue ValueFromUInt64(uint64_t value) {
    return CelValue::CreateUint64(value);
  }

  CelValue ValueFromMessage(const UInt64Value* wrapper) {
    return ValueFromUInt64(wrapper->value());
  }

  CelValue ValueFromFloat(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetFloatValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    return ValueFromFloat(reflection.GetValue(*message));
  }

  static CelValue ValueFromFloat(float value) {
    return CelValue::CreateDouble(value);
  }

  CelValue ValueFromMessage(const FloatValue* wrapper) {
    return ValueFromFloat(wrapper->value());
  }

  CelValue ValueFromDouble(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetDoubleValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    return ValueFromDouble(reflection.GetValue(*message));
  }

  static CelValue ValueFromDouble(double value) {
    return CelValue::CreateDouble(value);
  }

  CelValue ValueFromMessage(const DoubleValue* wrapper) {
    return ValueFromDouble(wrapper->value());
  }

  CelValue ValueFromString(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetStringValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    std::string scratch;
    return absl::visit(
        absl::Overload(
            [&](absl::string_view string) -> CelValue {
              if (string.data() == scratch.data() &&
                  string.size() == scratch.size()) {
                return CelValue::CreateString(
                    google::protobuf::Arena::Create<std::string>(arena_,
                                                       std::move(scratch)));
              }
              return CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
                  arena_, std::string(string)));
            },
            [&](absl::Cord&& cord) -> CelValue {
              auto* string = google::protobuf::Arena::Create<std::string>(arena_);
              absl::CopyCordToString(cord, string);
              return CelValue::CreateString(string);
            }),
        cel::well_known_types::AsVariant(
            reflection.GetValue(*message, scratch)));
  }

  CelValue ValueFromString(const absl::Cord& value) {
    return CelValue::CreateString(
        Arena::Create<std::string>(arena_, static_cast<std::string>(value)));
  }

  static CelValue ValueFromString(const std::string* value) {
    return CelValue::CreateString(value);
  }

  CelValue ValueFromMessage(const StringValue* wrapper) {
    return ValueFromString(&wrapper->value());
  }

  CelValue ValueFromBytes(const google::protobuf::Message* message) {
    CEL_ASSIGN_OR_RETURN(auto reflection,
                         cel::well_known_types::GetBytesValueReflection(
                             message->GetDescriptor()),
                         _.With(ReturnCelValueError(arena_)));
    std::string scratch;
    return absl::visit(
        absl::Overload(
            [&](absl::string_view string) -> CelValue {
              if (string.data() == scratch.data() &&
                  string.size() == scratch.size()) {
                return CelValue::CreateBytes(google::protobuf::Arena::Create<std::string>(
                    arena_, std::move(scratch)));
              }
              return CelValue::CreateBytes(google::protobuf::Arena::Create<std::string>(
                  arena_, std::string(string)));
            },
            [&](absl::Cord&& cord) -> CelValue {
              auto* string = google::protobuf::Arena::Create<std::string>(arena_);
              absl::CopyCordToString(cord, string);
              return CelValue::CreateBytes(string);
            }),
        cel::well_known_types::AsVariant(
            reflection.GetValue(*message, scratch)));
  }

  CelValue ValueFromBytes(const absl::Cord& value) {
    return CelValue::CreateBytes(
        Arena::Create<std::string>(arena_, static_cast<std::string>(value)));
  }

  static CelValue ValueFromBytes(google::protobuf::Arena* arena, std::string value) {
    return CelValue::CreateBytes(
        Arena::Create<std::string>(arena, std::move(value)));
  }

  CelValue ValueFromMessage(const BytesValue* wrapper) {
    // BytesValue stores value as Cord
    return CelValue::CreateBytes(
        Arena::Create<std::string>(arena_, std::string(wrapper->value())));
  }

  CelValue ValueFromMessage(const Value* value) {
    switch (value->kind_case()) {
      case Value::KindCase::kNullValue:
        return CelValue::CreateNull();
      case Value::KindCase::kNumberValue:
        return CelValue::CreateDouble(value->number_value());
      case Value::KindCase::kStringValue:
        return CelValue::CreateString(&value->string_value());
      case Value::KindCase::kBoolValue:
        return CelValue::CreateBool(value->bool_value());
      case Value::KindCase::kStructValue:
        return ValueFromMessage(&value->struct_value());
      case Value::KindCase::kListValue:
        return ValueFromMessage(&value->list_value());
      default:
        return CelValue::CreateNull();
    }
  }

  template <typename T>
  CelValue ValueFromGeneratedMessageLite(const google::protobuf::Message* message) {
    const auto* downcast_message = google::protobuf::DynamicCastToGenerated<T>(message);
    if (downcast_message != nullptr) {
      return ValueFromMessage(downcast_message);
    }
    auto* value = google::protobuf::Arena::Create<T>(arena_);
    absl::Cord serialized;
    if (!message->SerializeToCord(&serialized)) {
      return CreateErrorValue(
          arena_, absl::UnknownError(
                      absl::StrCat("failed to serialize dynamic message: ",
                                   message->GetTypeName())));
    }
    if (!value->ParseFromCord(serialized)) {
      return CreateErrorValue(arena_, absl::UnknownError(absl::StrCat(
                                          "failed to parse generated message: ",
                                          value->GetTypeName())));
    }
    return ValueFromMessage(value);
  }

  template <typename T>
  CelValue ValueFromMessage(const google::protobuf::Message* message) {
    if constexpr (std::is_same_v<Any, T>) {
      return ValueFromAny(message);
    } else if constexpr (std::is_same_v<BoolValue, T>) {
      return ValueFromBool(message);
    } else if constexpr (std::is_same_v<BytesValue, T>) {
      return ValueFromBytes(message);
    } else if constexpr (std::is_same_v<DoubleValue, T>) {
      return ValueFromDouble(message);
    } else if constexpr (std::is_same_v<Duration, T>) {
      return ValueFromDuration(message);
    } else if constexpr (std::is_same_v<FloatValue, T>) {
      return ValueFromFloat(message);
    } else if constexpr (std::is_same_v<Int32Value, T>) {
      return ValueFromInt32(message);
    } else if constexpr (std::is_same_v<Int64Value, T>) {
      return ValueFromInt64(message);
    } else if constexpr (std::is_same_v<ListValue, T>) {
      return ValueFromGeneratedMessageLite<ListValue>(message);
    } else if constexpr (std::is_same_v<StringValue, T>) {
      return ValueFromString(message);
    } else if constexpr (std::is_same_v<Struct, T>) {
      return ValueFromGeneratedMessageLite<Struct>(message);
    } else if constexpr (std::is_same_v<Timestamp, T>) {
      return ValueFromTimestamp(message);
    } else if constexpr (std::is_same_v<UInt32Value, T>) {
      return ValueFromUInt32(message);
    } else if constexpr (std::is_same_v<UInt64Value, T>) {
      return ValueFromUInt64(message);
    } else if constexpr (std::is_same_v<Value, T>) {
      return ValueFromGeneratedMessageLite<Value>(message);
    } else {
      ABSL_UNREACHABLE();
    }
  }

 private:
  const ProtobufValueFactory& value_factory_;
  const google::protobuf::DescriptorPool* descriptor_pool_;
  google::protobuf::Arena* arena_;
  MessageFactory* message_factory_;
};

// Class makes CelValue from generic protobuf Message.
// It holds a registry of CelValue factories for specific subtypes of Message.
// If message does not match any of types stored in registry, generic
// message-containing CelValue is created.
class ValueFromMessageMaker {
 public:
  template <class MessageType>
  static CelValue CreateWellknownTypeValue(const google::protobuf::Message* msg,
                                           const ProtobufValueFactory& factory,
                                           Arena* arena) {
    // Copy the original descriptor pool and message factory for unpacking 'Any'
    // values.
    google::protobuf::MessageFactory* message_factory =
        msg->GetReflection()->GetMessageFactory();
    const google::protobuf::DescriptorPool* pool = msg->GetDescriptor()->file()->pool();
    return ValueManager(factory, pool, arena, message_factory)
        .ValueFromMessage<MessageType>(msg);
  }

  static absl::optional<CelValue> CreateValue(
      const google::protobuf::Message* message, const ProtobufValueFactory& factory,
      Arena* arena) {
    switch (message->GetDescriptor()->well_known_type()) {
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
        return CreateWellknownTypeValue<DoubleValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
        return CreateWellknownTypeValue<FloatValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
        return CreateWellknownTypeValue<Int64Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
        return CreateWellknownTypeValue<UInt64Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
        return CreateWellknownTypeValue<Int32Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
        return CreateWellknownTypeValue<UInt32Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
        return CreateWellknownTypeValue<StringValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
        return CreateWellknownTypeValue<BytesValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
        return CreateWellknownTypeValue<BoolValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
        return CreateWellknownTypeValue<Any>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION:
        return CreateWellknownTypeValue<Duration>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP:
        return CreateWellknownTypeValue<Timestamp>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
        return CreateWellknownTypeValue<Value>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
        return CreateWellknownTypeValue<ListValue>(message, factory, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
        return CreateWellknownTypeValue<Struct>(message, factory, arena);
      // WELLKNOWNTYPE_FIELDMASK has no special CelValue type
      default:
        return absl::nullopt;
    }
  }

  // Non-copyable, non-assignable
  ValueFromMessageMaker(const ValueFromMessageMaker&) = delete;
  ValueFromMessageMaker& operator=(const ValueFromMessageMaker&) = delete;
};

CelValue DynamicList::operator[](int index) const {
  return ValueManager(factory_, arena_)
      .ValueFromMessage(&values_->values(index));
}

absl::optional<CelValue> DynamicMap::operator[](CelValue key) const {
  CelValue::StringHolder str_key;
  if (!key.GetValue(&str_key)) {
    // Not a string key.
    return CreateErrorValue(arena_, absl::InvalidArgumentError(absl::StrCat(
                                        "Invalid map key type: '",
                                        CelValue::TypeName(key.type()), "'")));
  }

  auto it = values_->fields().find(std::string(str_key.value()));
  if (it == values_->fields().end()) {
    return absl::nullopt;
  }

  return ValueManager(factory_, arena_).ValueFromMessage(&it->second);
}

google::protobuf::Message* DurationFromValue(const google::protobuf::Message* prototype,
                                   const CelValue& value,
                                   google::protobuf::Arena* arena) {
  absl::Duration val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  if (!cel::internal::ValidateDuration(val).ok()) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetDurationReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.UnsafeSetFromAbslDuration(message, val);
  return message;
}

google::protobuf::Message* BoolFromValue(const google::protobuf::Message* prototype,
                               const CelValue& value, google::protobuf::Arena* arena) {
  bool val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetBoolValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, val);
  return message;
}

google::protobuf::Message* BytesFromValue(const google::protobuf::Message* prototype,
                                const CelValue& value, google::protobuf::Arena* arena) {
  CelValue::BytesHolder view_val;
  if (!value.GetValue(&view_val)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetBytesValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, view_val.value());
  return message;
}

google::protobuf::Message* DoubleFromValue(const google::protobuf::Message* prototype,
                                 const CelValue& value, google::protobuf::Arena* arena) {
  double val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetDoubleValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, val);
  return message;
}

google::protobuf::Message* FloatFromValue(const google::protobuf::Message* prototype,
                                const CelValue& value, google::protobuf::Arena* arena) {
  double val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  float fval = val;
  // Abort the conversion if the value is outside the float range.
  if (val > std::numeric_limits<float>::max()) {
    fval = std::numeric_limits<float>::infinity();
  } else if (val < std::numeric_limits<float>::lowest()) {
    fval = -std::numeric_limits<float>::infinity();
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetFloatValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, static_cast<float>(fval));
  return message;
}

google::protobuf::Message* Int32FromValue(const google::protobuf::Message* prototype,
                                const CelValue& value, google::protobuf::Arena* arena) {
  int64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  if (!cel::internal::CheckedInt64ToInt32(val).ok()) {
    return nullptr;
  }
  int32_t ival = static_cast<int32_t>(val);
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetInt32ValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, ival);
  return message;
}

google::protobuf::Message* Int64FromValue(const google::protobuf::Message* prototype,
                                const CelValue& value, google::protobuf::Arena* arena) {
  int64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetInt64ValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, val);
  return message;
}

google::protobuf::Message* StringFromValue(const google::protobuf::Message* prototype,
                                 const CelValue& value, google::protobuf::Arena* arena) {
  CelValue::StringHolder view_val;
  if (!value.GetValue(&view_val)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetStringValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, view_val.value());
  return message;
}

google::protobuf::Message* TimestampFromValue(const google::protobuf::Message* prototype,
                                    const CelValue& value,
                                    google::protobuf::Arena* arena) {
  absl::Time val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  if (!cel::internal::ValidateTimestamp(val).ok()) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetTimestampReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.UnsafeSetFromAbslTime(message, val);
  return message;
}

google::protobuf::Message* UInt32FromValue(const google::protobuf::Message* prototype,
                                 const CelValue& value, google::protobuf::Arena* arena) {
  uint64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  if (!cel::internal::CheckedUint64ToUint32(val).ok()) {
    return nullptr;
  }
  uint32_t ival = static_cast<uint32_t>(val);
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetUInt32ValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, ival);
  return message;
}

google::protobuf::Message* UInt64FromValue(const google::protobuf::Message* prototype,
                                 const CelValue& value, google::protobuf::Arena* arena) {
  uint64_t val;
  if (!value.GetValue(&val)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetUInt64ValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetValue(message, val);
  return message;
}

google::protobuf::Message* ValueFromValue(google::protobuf::Message* message, const CelValue& value,
                                google::protobuf::Arena* arena);

google::protobuf::Message* ValueFromValue(const google::protobuf::Message* prototype,
                                const CelValue& value, google::protobuf::Arena* arena) {
  return ValueFromValue(prototype->New(arena), value, arena);
}

google::protobuf::Message* ListFromValue(google::protobuf::Message* message, const CelValue& value,
                               google::protobuf::Arena* arena) {
  if (!value.IsList()) {
    return nullptr;
  }
  const CelList& list = *value.ListOrDie();
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetListValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  for (int i = 0; i < list.size(); i++) {
    auto e = list.Get(arena, i);
    auto* elem = reflection.AddValues(message);
    if (ValueFromValue(elem, e, arena) == nullptr) {
      return nullptr;
    }
  }
  return message;
}

google::protobuf::Message* ListFromValue(const google::protobuf::Message* prototype,
                               const CelValue& value, google::protobuf::Arena* arena) {
  if (!value.IsList()) {
    return nullptr;
  }
  return ListFromValue(prototype->New(arena), value, arena);
}

google::protobuf::Message* StructFromValue(google::protobuf::Message* message,
                                 const CelValue& value, google::protobuf::Arena* arena) {
  if (!value.IsMap()) {
    return nullptr;
  }
  const CelMap& map = *value.MapOrDie();
  absl::StatusOr<const CelList*> keys_or = map.ListKeys(arena);
  if (!keys_or.ok()) {
    // If map doesn't support listing keys, it can't pack into a Struct value.
    // This will surface as a CEL error when the object creation expression
    // fails.
    return nullptr;
  }
  const CelList& keys = **keys_or;
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetStructReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  for (int i = 0; i < keys.size(); i++) {
    auto k = keys.Get(arena, i);
    // If the key is not a string type, abort the conversion.
    if (!k.IsString()) {
      return nullptr;
    }
    absl::string_view key = k.StringOrDie().value();

    auto v = map.Get(arena, k);
    if (!v.has_value()) {
      return nullptr;
    }
    auto* field = reflection.InsertField(message, key);
    if (ValueFromValue(field, *v, arena) == nullptr) {
      return nullptr;
    }
  }
  return message;
}

google::protobuf::Message* StructFromValue(const google::protobuf::Message* prototype,
                                 const CelValue& value, google::protobuf::Arena* arena) {
  if (!value.IsMap()) {
    return nullptr;
  }
  return StructFromValue(prototype->New(arena), value, arena);
}

google::protobuf::Message* ValueFromValue(google::protobuf::Message* message, const CelValue& value,
                                google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetValueReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  switch (value.type()) {
    case CelValue::Type::kBool: {
      bool val;
      if (value.GetValue(&val)) {
        reflection.SetBoolValue(message, val);
        return message;
      }
    } break;
    case CelValue::Type::kBytes: {
      // Base64 encode byte strings to ensure they can safely be transported
      // in a JSON string.
      CelValue::BytesHolder val;
      if (value.GetValue(&val)) {
        reflection.SetStringValueFromBytes(message, val.value());
        return message;
      }
    } break;
    case CelValue::Type::kDouble: {
      double val;
      if (value.GetValue(&val)) {
        reflection.SetNumberValue(message, val);
        return message;
      }
    } break;
    case CelValue::Type::kDuration: {
      // Convert duration values to a protobuf JSON format.
      absl::Duration val;
      if (value.GetValue(&val)) {
        CEL_RETURN_IF_ERROR(cel::internal::ValidateDuration(val))
            .With(IgnoreErrorAndReturnNullptr());
        reflection.SetStringValueFromDuration(message, val);
        return message;
      }
    } break;
    case CelValue::Type::kInt64: {
      int64_t val;
      // Convert int64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (value.GetValue(&val)) {
        reflection.SetNumberValue(message, val);
        return message;
      }
    } break;
    case CelValue::Type::kString: {
      CelValue::StringHolder val;
      if (value.GetValue(&val)) {
        reflection.SetStringValue(message, val.value());
        return message;
      }
    } break;
    case CelValue::Type::kTimestamp: {
      // Convert timestamp values to a protobuf JSON format.
      absl::Time val;
      if (value.GetValue(&val)) {
        CEL_RETURN_IF_ERROR(cel::internal::ValidateTimestamp(val))
            .With(IgnoreErrorAndReturnNullptr());
        reflection.SetStringValueFromTimestamp(message, val);
        return message;
      }
    } break;
    case CelValue::Type::kUint64: {
      uint64_t val;
      // Convert uint64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (value.GetValue(&val)) {
        reflection.SetNumberValue(message, val);
        return message;
      }
    } break;
    case CelValue::Type::kList: {
      if (ListFromValue(reflection.MutableListValue(message), value, arena) !=
          nullptr) {
        return message;
      }
    } break;
    case CelValue::Type::kMap: {
      if (StructFromValue(reflection.MutableStructValue(message), value,
                          arena) != nullptr) {
        return message;
      }
    } break;
    case CelValue::Type::kNullType:
      reflection.SetNullValue(message);
      return message;
      break;
    default:
      return nullptr;
  }
  return nullptr;
}

bool ValueFromValue(Value* json, const CelValue& value, google::protobuf::Arena* arena);

bool ListFromValue(ListValue* json_list, const CelValue& value,
                   google::protobuf::Arena* arena) {
  if (!value.IsList()) {
    return false;
  }
  const CelList& list = *value.ListOrDie();
  for (int i = 0; i < list.size(); i++) {
    auto e = list.Get(arena, i);
    Value* elem = json_list->add_values();
    if (!ValueFromValue(elem, e, arena)) {
      return false;
    }
  }
  return true;
}

bool StructFromValue(Struct* json_struct, const CelValue& value,
                     google::protobuf::Arena* arena) {
  if (!value.IsMap()) {
    return false;
  }
  const CelMap& map = *value.MapOrDie();
  absl::StatusOr<const CelList*> keys_or = map.ListKeys(arena);
  if (!keys_or.ok()) {
    // If map doesn't support listing keys, it can't pack into a Struct value.
    // This will surface as a CEL error when the object creation expression
    // fails.
    return false;
  }
  const CelList& keys = **keys_or;
  auto fields = json_struct->mutable_fields();
  for (int i = 0; i < keys.size(); i++) {
    auto k = keys.Get(arena, i);
    // If the key is not a string type, abort the conversion.
    if (!k.IsString()) {
      return false;
    }
    absl::string_view key = k.StringOrDie().value();

    auto v = map.Get(arena, k);
    if (!v.has_value()) {
      return false;
    }
    Value field_value;
    if (!ValueFromValue(&field_value, *v, arena)) {
      return false;
    }
    (*fields)[std::string(key)] = field_value;
  }
  return true;
}

bool ValueFromValue(Value* json, const CelValue& value, google::protobuf::Arena* arena) {
  switch (value.type()) {
    case CelValue::Type::kBool: {
      bool val;
      if (value.GetValue(&val)) {
        json->set_bool_value(val);
        return true;
      }
    } break;
    case CelValue::Type::kBytes: {
      // Base64 encode byte strings to ensure they can safely be transported
      // in a JSON string.
      CelValue::BytesHolder val;
      if (value.GetValue(&val)) {
        json->set_string_value(absl::Base64Escape(val.value()));
        return true;
      }
    } break;
    case CelValue::Type::kDouble: {
      double val;
      if (value.GetValue(&val)) {
        json->set_number_value(val);
        return true;
      }
    } break;
    case CelValue::Type::kDuration: {
      // Convert duration values to a protobuf JSON format.
      absl::Duration val;
      if (value.GetValue(&val)) {
        auto encode = cel::internal::EncodeDurationToString(val);
        if (!encode.ok()) {
          return false;
        }
        json->set_string_value(*encode);
        return true;
      }
    } break;
    case CelValue::Type::kInt64: {
      int64_t val;
      // Convert int64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (value.GetValue(&val)) {
        if (IsJSONSafe(val)) {
          json->set_number_value(val);
        } else {
          json->set_string_value(absl::StrCat(val));
        }
        return true;
      }
    } break;
    case CelValue::Type::kString: {
      CelValue::StringHolder val;
      if (value.GetValue(&val)) {
        json->set_string_value(val.value());
        return true;
      }
    } break;
    case CelValue::Type::kTimestamp: {
      // Convert timestamp values to a protobuf JSON format.
      absl::Time val;
      if (value.GetValue(&val)) {
        auto encode = cel::internal::EncodeTimeToString(val);
        if (!encode.ok()) {
          return false;
        }
        json->set_string_value(*encode);
        return true;
      }
    } break;
    case CelValue::Type::kUint64: {
      uint64_t val;
      // Convert uint64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (value.GetValue(&val)) {
        if (IsJSONSafe(val)) {
          json->set_number_value(val);
        } else {
          json->set_string_value(absl::StrCat(val));
        }
        return true;
      }
    } break;
    case CelValue::Type::kList:
      return ListFromValue(json->mutable_list_value(), value, arena);
    case CelValue::Type::kMap:
      return StructFromValue(json->mutable_struct_value(), value, arena);
    case CelValue::Type::kNullType:
      json->set_null_value(protobuf::NULL_VALUE);
      return true;
    default:
      return false;
  }
  return false;
}

google::protobuf::Message* AnyFromValue(const google::protobuf::Message* prototype,
                              const CelValue& value, google::protobuf::Arena* arena) {
  std::string type_name;
  absl::Cord payload;

  // In open source, any->PackFrom() returns void rather than boolean.
  switch (value.type()) {
    case CelValue::Type::kBool: {
      BoolValue v;
      type_name = v.GetTypeName();
      v.set_value(value.BoolOrDie());
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kBytes: {
      BytesValue v;
      type_name = v.GetTypeName();
      v.set_value(std::string(value.BytesOrDie().value()));
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kDouble: {
      DoubleValue v;
      type_name = v.GetTypeName();
      v.set_value(value.DoubleOrDie());
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kDuration: {
      Duration v;
      if (!cel::internal::EncodeDuration(value.DurationOrDie(), &v).ok()) {
        return nullptr;
      }
      type_name = v.GetTypeName();
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kInt64: {
      Int64Value v;
      type_name = v.GetTypeName();
      v.set_value(value.Int64OrDie());
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kString: {
      StringValue v;
      type_name = v.GetTypeName();
      v.set_value(std::string(value.StringOrDie().value()));
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kTimestamp: {
      Timestamp v;
      if (!cel::internal::EncodeTime(value.TimestampOrDie(), &v).ok()) {
        return nullptr;
      }
      type_name = v.GetTypeName();
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kUint64: {
      UInt64Value v;
      type_name = v.GetTypeName();
      v.set_value(value.Uint64OrDie());
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kList: {
      ListValue v;
      if (!ListFromValue(&v, value, arena)) {
        return nullptr;
      }
      type_name = v.GetTypeName();
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kMap: {
      Struct v;
      if (!StructFromValue(&v, value, arena)) {
        return nullptr;
      }
      type_name = v.GetTypeName();
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kNullType: {
      Value v;
      type_name = v.GetTypeName();
      v.set_null_value(google::protobuf::NULL_VALUE);
      payload = v.SerializeAsCord();
    } break;
    case CelValue::Type::kMessage: {
      type_name = value.MessageWrapperOrDie().message_ptr()->GetTypeName();
      payload = value.MessageWrapperOrDie().message_ptr()->SerializeAsCord();
    } break;
    default:
      return nullptr;
  }

  auto* message = prototype->New(arena);
  CEL_ASSIGN_OR_RETURN(
      auto reflection,
      cel::well_known_types::GetAnyReflection(message->GetDescriptor()),
      _.With(IgnoreErrorAndReturnNullptr()));
  reflection.SetTypeUrl(message,
                        absl::StrCat("type.googleapis.com/", type_name));
  reflection.SetValue(message, payload);
  return message;
}

bool IsAlreadyWrapped(google::protobuf::Descriptor::WellKnownType wkt,
                      const CelValue& value) {
  if (value.IsMessage()) {
    const auto* msg = value.MessageOrDie();
    if (wkt == msg->GetDescriptor()->well_known_type()) {
      return true;
    }
  }
  return false;
}

// MessageFromValueMaker makes a specific protobuf Message instance based on
// the desired protobuf type name and an input CelValue.
//
// It holds a registry of CelValue factories for specific subtypes of Message.
// If message does not match any of types stored in registry, an the factory
// returns an absent value.
class MessageFromValueMaker {
 public:
  // Non-copyable, non-assignable
  MessageFromValueMaker(const MessageFromValueMaker&) = delete;
  MessageFromValueMaker& operator=(const MessageFromValueMaker&) = delete;

  static google::protobuf::Message* MaybeWrapMessage(const google::protobuf::Descriptor* descriptor,
                                           google::protobuf::MessageFactory* factory,
                                           const CelValue& value,
                                           Arena* arena) {
    switch (descriptor->well_known_type()) {
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return DoubleFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return FloatFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return Int64FromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return UInt64FromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return Int32FromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return UInt32FromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return StringFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return BytesFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return BoolFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return AnyFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return DurationFromValue(factory->GetPrototype(descriptor), value,
                                 arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return TimestampFromValue(factory->GetPrototype(descriptor), value,
                                  arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return ValueFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return ListFromValue(factory->GetPrototype(descriptor), value, arena);
      case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
        if (IsAlreadyWrapped(descriptor->well_known_type(), value)) {
          return nullptr;
        }
        return StructFromValue(factory->GetPrototype(descriptor), value, arena);
      // WELLKNOWNTYPE_FIELDMASK has no special CelValue type
      default:
        return nullptr;
    }
  }
};

}  // namespace

CelValue UnwrapMessageToValue(const google::protobuf::Message* value,
                              const ProtobufValueFactory& factory,
                              Arena* arena) {
  // Messages are Nullable types
  if (value == nullptr) {
    return CelValue::CreateNull();
  }

  absl::optional<CelValue> special_value =
      ValueFromMessageMaker::CreateValue(value, factory, arena);
  if (special_value.has_value()) {
    return *special_value;
  }
  return factory(value);
}

const google::protobuf::Message* MaybeWrapValueToMessage(
    const google::protobuf::Descriptor* descriptor, google::protobuf::MessageFactory* factory,
    const CelValue& value, Arena* arena) {
  google::protobuf::Message* msg = MessageFromValueMaker::MaybeWrapMessage(
      descriptor, factory, value, arena);
  return msg;
}

}  // namespace google::api::expr::runtime::internal
