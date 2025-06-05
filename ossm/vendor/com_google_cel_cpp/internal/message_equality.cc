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

#include "internal/message_equality.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/memory.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/json.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel::internal {

namespace {

using ::cel::extensions::protobuf_internal::LookupMapValue;
using ::cel::extensions::protobuf_internal::MapBegin;
using ::cel::extensions::protobuf_internal::MapEnd;
using ::cel::extensions::protobuf_internal::MapSize;
using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Message;
using ::google::protobuf::MessageFactory;
using ::google::protobuf::util::MessageDifferencer;

class EquatableListValue final
    : public std::reference_wrapper<const google::protobuf::Message> {
 public:
  using std::reference_wrapper<const google::protobuf::Message>::reference_wrapper;
};

class EquatableStruct final
    : public std::reference_wrapper<const google::protobuf::Message> {
 public:
  using std::reference_wrapper<const google::protobuf::Message>::reference_wrapper;
};

class EquatableAny final
    : public std::reference_wrapper<const google::protobuf::Message> {
 public:
  using std::reference_wrapper<const google::protobuf::Message>::reference_wrapper;
};

class EquatableMessage final
    : public std::reference_wrapper<const google::protobuf::Message> {
 public:
  using std::reference_wrapper<const google::protobuf::Message>::reference_wrapper;
};

using EquatableValue =
    absl::variant<std::nullptr_t, bool, int64_t, uint64_t, double,
                  well_known_types::BytesValue, well_known_types::StringValue,
                  absl::Duration, absl::Time, EquatableListValue,
                  EquatableStruct, EquatableAny, EquatableMessage>;

struct NullValueEqualer {
  bool operator()(std::nullptr_t, std::nullptr_t) const { return true; }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<std::nullptr_t, T>>, bool>
  operator()(std::nullptr_t, const T&) const {
    return false;
  }
};

struct BoolValueEqualer {
  bool operator()(bool lhs, bool rhs) const { return lhs == rhs; }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<bool, T>>, bool> operator()(
      bool, const T&) const {
    return false;
  }
};

struct BytesValueEqualer {
  bool operator()(const well_known_types::BytesValue& lhs,
                  const well_known_types::BytesValue& rhs) const {
    return lhs == rhs;
  }

  template <typename T>
  std::enable_if_t<
      std::negation_v<std::is_same<well_known_types::BytesValue, T>>, bool>
  operator()(const well_known_types::BytesValue&, const T&) const {
    return false;
  }
};

struct IntValueEqualer {
  bool operator()(int64_t lhs, int64_t rhs) const { return lhs == rhs; }

  bool operator()(int64_t lhs, uint64_t rhs) const {
    return Number::FromInt64(lhs) == Number::FromUint64(rhs);
  }

  bool operator()(int64_t lhs, double rhs) const {
    return Number::FromInt64(lhs) == Number::FromDouble(rhs);
  }

  template <typename T>
  std::enable_if_t<std::conjunction_v<std::negation<std::is_same<int64_t, T>>,
                                      std::negation<std::is_same<uint64_t, T>>,
                                      std::negation<std::is_same<double, T>>>,
                   bool>
  operator()(int64_t, const T&) const {
    return false;
  }
};

struct UintValueEqualer {
  bool operator()(uint64_t lhs, int64_t rhs) const {
    return Number::FromUint64(lhs) == Number::FromInt64(rhs);
  }

  bool operator()(uint64_t lhs, uint64_t rhs) const { return lhs == rhs; }

  bool operator()(uint64_t lhs, double rhs) const {
    return Number::FromUint64(lhs) == Number::FromDouble(rhs);
  }

  template <typename T>
  std::enable_if_t<std::conjunction_v<std::negation<std::is_same<int64_t, T>>,
                                      std::negation<std::is_same<uint64_t, T>>,
                                      std::negation<std::is_same<double, T>>>,
                   bool>
  operator()(uint64_t, const T&) const {
    return false;
  }
};

struct DoubleValueEqualer {
  bool operator()(double lhs, int64_t rhs) const {
    return Number::FromDouble(lhs) == Number::FromInt64(rhs);
  }

  bool operator()(double lhs, uint64_t rhs) const {
    return Number::FromDouble(lhs) == Number::FromUint64(rhs);
  }

  bool operator()(double lhs, double rhs) const { return lhs == rhs; }

  template <typename T>
  std::enable_if_t<std::conjunction_v<std::negation<std::is_same<int64_t, T>>,
                                      std::negation<std::is_same<uint64_t, T>>,
                                      std::negation<std::is_same<double, T>>>,
                   bool>
  operator()(double, const T&) const {
    return false;
  }
};

struct StringValueEqualer {
  bool operator()(const well_known_types::StringValue& lhs,
                  const well_known_types::StringValue& rhs) const {
    return lhs == rhs;
  }

  template <typename T>
  std::enable_if_t<
      std::negation_v<std::is_same<well_known_types::StringValue, T>>, bool>
  operator()(const well_known_types::StringValue&, const T&) const {
    return false;
  }
};

struct DurationEqualer {
  bool operator()(absl::Duration lhs, absl::Duration rhs) const {
    return lhs == rhs;
  }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<absl::Duration, T>>, bool>
  operator()(absl::Duration, const T&) const {
    return false;
  }
};

struct TimestampEqualer {
  bool operator()(absl::Time lhs, absl::Time rhs) const { return lhs == rhs; }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<absl::Time, T>>, bool>
  operator()(absl::Time, const T&) const {
    return false;
  }
};

struct ListValueEqualer {
  bool operator()(EquatableListValue lhs, EquatableListValue rhs) const {
    return JsonListEquals(lhs, rhs);
  }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<EquatableListValue, T>>, bool>
  operator()(EquatableListValue, const T&) const {
    return false;
  }
};

struct StructEqualer {
  bool operator()(EquatableStruct lhs, EquatableStruct rhs) const {
    return JsonMapEquals(lhs, rhs);
  }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<EquatableStruct, T>>, bool>
  operator()(EquatableStruct, const T&) const {
    return false;
  }
};

struct AnyEqualer {
  bool operator()(EquatableAny lhs, EquatableAny rhs) const {
    auto lhs_reflection =
        well_known_types::GetAnyReflectionOrDie(lhs.get().GetDescriptor());
    std::string lhs_type_url_scratch;
    std::string lhs_value_scratch;
    auto rhs_reflection =
        well_known_types::GetAnyReflectionOrDie(rhs.get().GetDescriptor());
    std::string rhs_type_url_scratch;
    std::string rhs_value_scratch;
    return lhs_reflection.GetTypeUrl(lhs.get(), lhs_type_url_scratch) ==
               rhs_reflection.GetTypeUrl(rhs.get(), rhs_type_url_scratch) &&
           lhs_reflection.GetValue(lhs.get(), lhs_value_scratch) ==
               rhs_reflection.GetValue(rhs.get(), rhs_value_scratch);
  }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<EquatableAny, T>>, bool>
  operator()(EquatableAny, const T&) const {
    return false;
  }
};

struct MessageEqualer {
  bool operator()(EquatableMessage lhs, EquatableMessage rhs) const {
    return lhs.get().GetDescriptor() == rhs.get().GetDescriptor() &&
           MessageDifferencer::Equals(lhs.get(), rhs.get());
  }

  template <typename T>
  std::enable_if_t<std::negation_v<std::is_same<EquatableMessage, T>>, bool>
  operator()(EquatableMessage, const T&) const {
    return false;
  }
};

struct EquatableValueReflection final {
  well_known_types::DoubleValueReflection double_value_reflection;
  well_known_types::FloatValueReflection float_value_reflection;
  well_known_types::Int64ValueReflection int64_value_reflection;
  well_known_types::UInt64ValueReflection uint64_value_reflection;
  well_known_types::Int32ValueReflection int32_value_reflection;
  well_known_types::UInt32ValueReflection uint32_value_reflection;
  well_known_types::StringValueReflection string_value_reflection;
  well_known_types::BytesValueReflection bytes_value_reflection;
  well_known_types::BoolValueReflection bool_value_reflection;
  well_known_types::AnyReflection any_reflection;
  well_known_types::DurationReflection duration_reflection;
  well_known_types::TimestampReflection timestamp_reflection;
  well_known_types::ValueReflection value_reflection;
  well_known_types::ListValueReflection list_value_reflection;
  well_known_types::StructReflection struct_reflection;
};

absl::StatusOr<EquatableValue> AsEquatableValue(
    EquatableValueReflection& reflection,
    const Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const Descriptor*> descriptor,
    Descriptor::WellKnownType well_known_type,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  switch (well_known_type) {
    case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      CEL_RETURN_IF_ERROR(
          reflection.double_value_reflection.Initialize(descriptor));
      return reflection.double_value_reflection.GetValue(message);
    case Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      CEL_RETURN_IF_ERROR(
          reflection.float_value_reflection.Initialize(descriptor));
      return static_cast<double>(
          reflection.float_value_reflection.GetValue(message));
    case Descriptor::WELLKNOWNTYPE_INT64VALUE:
      CEL_RETURN_IF_ERROR(
          reflection.int64_value_reflection.Initialize(descriptor));
      return reflection.int64_value_reflection.GetValue(message);
    case Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      CEL_RETURN_IF_ERROR(
          reflection.uint64_value_reflection.Initialize(descriptor));
      return reflection.uint64_value_reflection.GetValue(message);
    case Descriptor::WELLKNOWNTYPE_INT32VALUE:
      CEL_RETURN_IF_ERROR(
          reflection.int32_value_reflection.Initialize(descriptor));
      return static_cast<int64_t>(
          reflection.int32_value_reflection.GetValue(message));
    case Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      CEL_RETURN_IF_ERROR(
          reflection.uint32_value_reflection.Initialize(descriptor));
      return static_cast<uint64_t>(
          reflection.uint32_value_reflection.GetValue(message));
    case Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      CEL_RETURN_IF_ERROR(
          reflection.string_value_reflection.Initialize(descriptor));
      return reflection.string_value_reflection.GetValue(message, scratch);
    case Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      CEL_RETURN_IF_ERROR(
          reflection.bytes_value_reflection.Initialize(descriptor));
      return reflection.bytes_value_reflection.GetValue(message, scratch);
    case Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      CEL_RETURN_IF_ERROR(
          reflection.bool_value_reflection.Initialize(descriptor));
      return reflection.bool_value_reflection.GetValue(message);
    case Descriptor::WELLKNOWNTYPE_VALUE: {
      CEL_RETURN_IF_ERROR(reflection.value_reflection.Initialize(descriptor));
      const auto kind_case = reflection.value_reflection.GetKindCase(message);
      switch (kind_case) {
        case google::protobuf::Value::KIND_NOT_SET:
          ABSL_FALLTHROUGH_INTENDED;
        case google::protobuf::Value::kNullValue:
          return nullptr;
        case google::protobuf::Value::kBoolValue:
          return reflection.value_reflection.GetBoolValue(message);
        case google::protobuf::Value::kNumberValue:
          return reflection.value_reflection.GetNumberValue(message);
        case google::protobuf::Value::kStringValue:
          return reflection.value_reflection.GetStringValue(message, scratch);
        case google::protobuf::Value::kListValue:
          return EquatableListValue(
              reflection.value_reflection.GetListValue(message));
        case google::protobuf::Value::kStructValue:
          return EquatableStruct(
              reflection.value_reflection.GetStructValue(message));
        default:
          return absl::InternalError(
              absl::StrCat("unexpected value kind case: ", kind_case));
      }
    }
    case Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return EquatableListValue(message);
    case Descriptor::WELLKNOWNTYPE_STRUCT:
      return EquatableStruct(message);
    case Descriptor::WELLKNOWNTYPE_DURATION:
      CEL_RETURN_IF_ERROR(
          reflection.duration_reflection.Initialize(descriptor));
      return reflection.duration_reflection.ToAbslDuration(message);
    case Descriptor::WELLKNOWNTYPE_TIMESTAMP:
      CEL_RETURN_IF_ERROR(
          reflection.timestamp_reflection.Initialize(descriptor));
      return reflection.timestamp_reflection.ToAbslTime(message);
    case Descriptor::WELLKNOWNTYPE_ANY:
      return EquatableAny(message);
    default:
      return EquatableMessage(message);
  }
}

absl::StatusOr<EquatableValue> AsEquatableValue(
    EquatableValueReflection& reflection,
    const Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const Descriptor*> descriptor,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return AsEquatableValue(reflection, message, descriptor,
                          descriptor->well_known_type(), scratch);
}

absl::StatusOr<EquatableValue> AsEquatableValue(
    EquatableValueReflection& reflection,
    const Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<const FieldDescriptor*> field,
    std::string& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(!field->is_repeated() && !field->is_map());
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return static_cast<int64_t>(
          message.GetReflection()->GetInt32(message, field));
    case FieldDescriptor::CPPTYPE_INT64:
      return message.GetReflection()->GetInt64(message, field);
    case FieldDescriptor::CPPTYPE_UINT32:
      return static_cast<uint64_t>(
          message.GetReflection()->GetUInt32(message, field));
    case FieldDescriptor::CPPTYPE_UINT64:
      return message.GetReflection()->GetUInt64(message, field);
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return message.GetReflection()->GetDouble(message, field);
    case FieldDescriptor::CPPTYPE_FLOAT:
      return static_cast<double>(
          message.GetReflection()->GetFloat(message, field));
    case FieldDescriptor::CPPTYPE_BOOL:
      return message.GetReflection()->GetBool(message, field);
    case FieldDescriptor::CPPTYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return nullptr;
      }
      return static_cast<int64_t>(
          message.GetReflection()->GetEnumValue(message, field));
    case FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == FieldDescriptor::TYPE_BYTES) {
        return well_known_types::GetBytesField(message, field, scratch);
      }
      return well_known_types::GetStringField(message, field, scratch);
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return AsEquatableValue(
          reflection, message.GetReflection()->GetMessage(message, field),
          field->message_type(), scratch);
    default:
      return absl::InternalError(
          absl::StrCat("unexpected field type: ", field->cpp_type_name()));
  }
}

bool IsAny(const Message& message) {
  return message.GetDescriptor()->well_known_type() ==
         Descriptor::WELLKNOWNTYPE_ANY;
}

bool IsAnyField(absl::Nonnull<const FieldDescriptor*> field) {
  return field->type() == FieldDescriptor::TYPE_MESSAGE &&
         field->message_type()->well_known_type() ==
             Descriptor::WELLKNOWNTYPE_ANY;
}

absl::StatusOr<EquatableValue> MapValueAsEquatableValue(
    absl::Nonnull<google::protobuf::Arena*> arena,
    absl::Nonnull<const DescriptorPool*> pool,
    absl::Nonnull<MessageFactory*> factory,
    EquatableValueReflection& reflection, const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const FieldDescriptor*> field, std::string& scratch,
    Unique<Message>& unpacked) {
  if (IsAnyField(field)) {
    CEL_ASSIGN_OR_RETURN(unpacked, well_known_types::UnpackAnyIfResolveable(
                                       arena, reflection.any_reflection,
                                       value.GetMessageValue(), pool, factory));
    if (unpacked) {
      return AsEquatableValue(reflection, *unpacked, unpacked->GetDescriptor(),
                              scratch);
    }
    return AsEquatableValue(reflection, value.GetMessageValue(),
                            value.GetMessageValue().GetDescriptor(), scratch);
  }
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return static_cast<int64_t>(value.GetInt32Value());
    case FieldDescriptor::CPPTYPE_INT64:
      return value.GetInt64Value();
    case FieldDescriptor::CPPTYPE_UINT32:
      return static_cast<uint64_t>(value.GetUInt32Value());
    case FieldDescriptor::CPPTYPE_UINT64:
      return value.GetUInt64Value();
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return value.GetDoubleValue();
    case FieldDescriptor::CPPTYPE_FLOAT:
      return static_cast<double>(value.GetFloatValue());
    case FieldDescriptor::CPPTYPE_BOOL:
      return value.GetBoolValue();
    case FieldDescriptor::CPPTYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return nullptr;
      }
      return static_cast<int64_t>(value.GetEnumValue());
    case FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == FieldDescriptor::TYPE_BYTES) {
        return well_known_types::BytesValue(
            absl::string_view(value.GetStringValue()));
      }
      return well_known_types::StringValue(
          absl::string_view(value.GetStringValue()));
    case FieldDescriptor::CPPTYPE_MESSAGE: {
      const auto& message = value.GetMessageValue();
      return AsEquatableValue(reflection, message, message.GetDescriptor(),
                              scratch);
    }
    default:
      return absl::InternalError(
          absl::StrCat("unexpected field type: ", field->cpp_type_name()));
  }
}

absl::StatusOr<EquatableValue> RepeatedFieldAsEquatableValue(
    absl::Nonnull<google::protobuf::Arena*> arena,
    absl::Nonnull<const DescriptorPool*> pool,
    absl::Nonnull<MessageFactory*> factory,
    EquatableValueReflection& reflection, const Message& message,
    absl::Nonnull<const FieldDescriptor*> field, int index,
    std::string& scratch, Unique<Message>& unpacked) {
  if (IsAnyField(field)) {
    const auto& field_value =
        message.GetReflection()->GetRepeatedMessage(message, field, index);
    CEL_ASSIGN_OR_RETURN(unpacked, well_known_types::UnpackAnyIfResolveable(
                                       arena, reflection.any_reflection,
                                       field_value, pool, factory));
    if (unpacked) {
      return AsEquatableValue(reflection, *unpacked, unpacked->GetDescriptor(),
                              scratch);
    }
    return AsEquatableValue(reflection, field_value,
                            field_value.GetDescriptor(), scratch);
  }
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return static_cast<int64_t>(
          message.GetReflection()->GetRepeatedInt32(message, field, index));
    case FieldDescriptor::CPPTYPE_INT64:
      return message.GetReflection()->GetRepeatedInt64(message, field, index);
    case FieldDescriptor::CPPTYPE_UINT32:
      return static_cast<uint64_t>(
          message.GetReflection()->GetRepeatedUInt32(message, field, index));
    case FieldDescriptor::CPPTYPE_UINT64:
      return message.GetReflection()->GetRepeatedUInt64(message, field, index);
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return message.GetReflection()->GetRepeatedDouble(message, field, index);
    case FieldDescriptor::CPPTYPE_FLOAT:
      return static_cast<double>(
          message.GetReflection()->GetRepeatedFloat(message, field, index));
    case FieldDescriptor::CPPTYPE_BOOL:
      return message.GetReflection()->GetRepeatedBool(message, field, index);
    case FieldDescriptor::CPPTYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return nullptr;
      }
      return static_cast<int64_t>(
          message.GetReflection()->GetRepeatedEnumValue(message, field, index));
    case FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == FieldDescriptor::TYPE_BYTES) {
        return well_known_types::GetRepeatedBytesField(message, field, index,
                                                       scratch);
      }
      return well_known_types::GetRepeatedStringField(message, field, index,
                                                      scratch);
    case FieldDescriptor::CPPTYPE_MESSAGE: {
      const auto& submessage =
          message.GetReflection()->GetRepeatedMessage(message, field, index);
      return AsEquatableValue(reflection, submessage,
                              submessage.GetDescriptor(), scratch);
    }
    default:
      return absl::InternalError(
          absl::StrCat("unexpected field type: ", field->cpp_type_name()));
  }
}

// Compare two `EquatableValue` for equality.
bool EquatableValueEquals(const EquatableValue& lhs,
                          const EquatableValue& rhs) {
  return absl::visit(
      absl::Overload(NullValueEqualer{}, BoolValueEqualer{},
                     BytesValueEqualer{}, IntValueEqualer{}, UintValueEqualer{},
                     DoubleValueEqualer{}, StringValueEqualer{},
                     DurationEqualer{}, TimestampEqualer{}, ListValueEqualer{},
                     StructEqualer{}, AnyEqualer{}, MessageEqualer{}),
      lhs, rhs);
}

// Attempts to coalesce one map key to another. Returns true if it was possible,
// false otherwise.
bool CoalesceMapKey(const google::protobuf::MapKey& src,
                    FieldDescriptor::CppType dest_type,
                    absl::Nonnull<google::protobuf::MapKey*> dest) {
  switch (src.type()) {
    case FieldDescriptor::CPPTYPE_BOOL:
      if (dest_type != FieldDescriptor::CPPTYPE_BOOL) {
        return false;
      }
      dest->SetBoolValue(src.GetBoolValue());
      return true;
    case FieldDescriptor::CPPTYPE_INT32: {
      const auto src_value = src.GetInt32Value();
      switch (dest_type) {
        case FieldDescriptor::CPPTYPE_INT32:
          dest->SetInt32Value(src_value);
          return true;
        case FieldDescriptor::CPPTYPE_INT64:
          dest->SetInt64Value(src_value);
          return true;
        case FieldDescriptor::CPPTYPE_UINT32:
          if (src_value < 0) {
            return false;
          }
          dest->SetUInt32Value(static_cast<uint32_t>(src_value));
          return true;
        case FieldDescriptor::CPPTYPE_UINT64:
          if (src_value < 0) {
            return false;
          }
          dest->SetUInt64Value(static_cast<uint64_t>(src_value));
          return true;
        default:
          return false;
      }
    }
    case FieldDescriptor::CPPTYPE_INT64: {
      const auto src_value = src.GetInt64Value();
      switch (dest_type) {
        case FieldDescriptor::CPPTYPE_INT32:
          if (src_value < std::numeric_limits<int32_t>::min() ||
              src_value > std::numeric_limits<int32_t>::max()) {
            return false;
          }
          dest->SetInt32Value(static_cast<int32_t>(src_value));
          return true;
        case FieldDescriptor::CPPTYPE_INT64:
          dest->SetInt64Value(src_value);
          return true;
        case FieldDescriptor::CPPTYPE_UINT32:
          if (src_value < 0 ||
              src_value > std::numeric_limits<uint32_t>::max()) {
            return false;
          }
          dest->SetUInt32Value(static_cast<uint32_t>(src_value));
          return true;
        case FieldDescriptor::CPPTYPE_UINT64:
          if (src_value < 0) {
            return false;
          }
          dest->SetUInt64Value(static_cast<uint64_t>(src_value));
          return true;
        default:
          return false;
      }
    }
    case FieldDescriptor::CPPTYPE_UINT32: {
      const auto src_value = src.GetUInt32Value();
      switch (dest_type) {
        case FieldDescriptor::CPPTYPE_INT32:
          if (src_value > std::numeric_limits<int32_t>::max()) {
            return false;
          }
          dest->SetInt32Value(static_cast<int32_t>(src_value));
          return true;
        case FieldDescriptor::CPPTYPE_INT64:
          dest->SetInt64Value(static_cast<int64_t>(src_value));
          return true;
        case FieldDescriptor::CPPTYPE_UINT32:
          dest->SetUInt32Value(src_value);
          return true;
        case FieldDescriptor::CPPTYPE_UINT64:
          dest->SetUInt64Value(static_cast<uint64_t>(src_value));
          return true;
        default:
          return false;
      }
    }
    case FieldDescriptor::CPPTYPE_UINT64: {
      const auto src_value = src.GetUInt64Value();
      switch (dest_type) {
        case FieldDescriptor::CPPTYPE_INT32:
          if (src_value > std::numeric_limits<int32_t>::max()) {
            return false;
          }
          dest->SetInt32Value(static_cast<int32_t>(src_value));
          return true;
        case FieldDescriptor::CPPTYPE_INT64:
          if (src_value > std::numeric_limits<int64_t>::max()) {
            return false;
          }
          dest->SetInt64Value(static_cast<int64_t>(src_value));
          return true;
        case FieldDescriptor::CPPTYPE_UINT32:
          if (src_value > std::numeric_limits<uint32_t>::max()) {
            return false;
          }
          dest->SetUInt32Value(src_value);
          return true;
        case FieldDescriptor::CPPTYPE_UINT64:
          dest->SetUInt64Value(src_value);
          return true;
        default:
          return false;
      }
    }
    case FieldDescriptor::CPPTYPE_STRING:
      if (dest_type != FieldDescriptor::CPPTYPE_STRING) {
        return false;
      }
      dest->SetStringValue(src.GetStringValue());
      return true;
    default:
      // Only bool, integrals, and string may be map keys.
      ABSL_UNREACHABLE();
  }
}

// Bits used for categorizing equality. Can be used to cheaply check whether two
// categories are comparable for equality by performing an AND and checking if
// the result against `kNone`.
enum class EquatableCategory {
  kNone = 0,

  kNullLike = 1 << 0,
  kBoolLike = 1 << 1,
  kNumericLike = 1 << 2,
  kBytesLike = 1 << 3,
  kStringLike = 1 << 4,
  kList = 1 << 5,
  kMap = 1 << 6,
  kMessage = 1 << 7,
  kDuration = 1 << 8,
  kTimestamp = 1 << 9,

  kAny = kNullLike | kBoolLike | kNumericLike | kBytesLike | kStringLike |
         kList | kMap | kMessage | kDuration | kTimestamp,
  kValue = kNullLike | kBoolLike | kNumericLike | kStringLike | kList | kMap,
};

constexpr EquatableCategory operator&(EquatableCategory lhs,
                                      EquatableCategory rhs) {
  return static_cast<EquatableCategory>(
      static_cast<std::underlying_type_t<EquatableCategory>>(lhs) &
      static_cast<std::underlying_type_t<EquatableCategory>>(rhs));
}

constexpr bool operator==(EquatableCategory lhs, EquatableCategory rhs) {
  return static_cast<std::underlying_type_t<EquatableCategory>>(lhs) ==
         static_cast<std::underlying_type_t<EquatableCategory>>(rhs);
}

EquatableCategory GetEquatableCategory(
    absl::Nonnull<const Descriptor*> descriptor) {
  switch (descriptor->well_known_type()) {
    case Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      return EquatableCategory::kBoolLike;
    case Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_INT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_INT64VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      return EquatableCategory::kNumericLike;
    case Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      return EquatableCategory::kBytesLike;
    case Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      return EquatableCategory::kStringLike;
    case Descriptor::WELLKNOWNTYPE_VALUE:
      return EquatableCategory::kValue;
    case Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return EquatableCategory::kList;
    case Descriptor::WELLKNOWNTYPE_STRUCT:
      return EquatableCategory::kMap;
    case Descriptor::WELLKNOWNTYPE_ANY:
      return EquatableCategory::kAny;
    case Descriptor::WELLKNOWNTYPE_DURATION:
      return EquatableCategory::kDuration;
    case Descriptor::WELLKNOWNTYPE_TIMESTAMP:
      return EquatableCategory::kTimestamp;
    default:
      return EquatableCategory::kAny;
  }
}

EquatableCategory GetEquatableFieldCategory(
    absl::Nonnull<const FieldDescriptor*> field) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_ENUM:
      return field->enum_type()->full_name() == "google.protobuf.NullValue"
                 ? EquatableCategory::kNullLike
                 : EquatableCategory::kNumericLike;
    case FieldDescriptor::CPPTYPE_BOOL:
      return EquatableCategory::kBoolLike;
    case FieldDescriptor::CPPTYPE_FLOAT:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::CPPTYPE_DOUBLE:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::CPPTYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::CPPTYPE_UINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::CPPTYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::CPPTYPE_UINT64:
      return EquatableCategory::kNumericLike;
    case FieldDescriptor::CPPTYPE_STRING:
      return field->type() == FieldDescriptor::TYPE_BYTES
                 ? EquatableCategory::kBytesLike
                 : EquatableCategory::kStringLike;
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return GetEquatableCategory(field->message_type());
    default:
      // Ugh. Force any future additions to compare instead of short circuiting.
      return EquatableCategory::kAny;
  }
}

class MessageEqualsState final {
 public:
  MessageEqualsState(absl::Nonnull<const DescriptorPool*> pool,
                     absl::Nonnull<MessageFactory*> factory)
      : pool_(pool), factory_(factory) {}

  // Equality between messages.
  absl::StatusOr<bool> Equals(const Message& lhs, const Message& rhs) {
    const auto* lhs_descriptor = lhs.GetDescriptor();
    const auto* rhs_descriptor = rhs.GetDescriptor();
    // Deal with well known types, starting with any.
    auto lhs_well_known_type = lhs_descriptor->well_known_type();
    auto rhs_well_known_type = rhs_descriptor->well_known_type();
    absl::Nonnull<const Message*> lhs_ptr = &lhs;
    absl::Nonnull<const Message*> rhs_ptr = &rhs;
    Unique<Message> lhs_unpacked;
    Unique<Message> rhs_unpacked;
    // Deal with any first. We could in theory check if we should bother
    // unpacking, but that is more complicated. We can always implement it
    // later.
    if (lhs_well_known_type == Descriptor::WELLKNOWNTYPE_ANY) {
      CEL_ASSIGN_OR_RETURN(
          lhs_unpacked,
          well_known_types::UnpackAnyIfResolveable(
              &arena_, lhs_reflection_.any_reflection, lhs, pool_, factory_));
      if (lhs_unpacked) {
        lhs_ptr = cel::to_address(lhs_unpacked);
        lhs_descriptor = lhs_ptr->GetDescriptor();
        lhs_well_known_type = lhs_descriptor->well_known_type();
      }
    }
    if (rhs_well_known_type == Descriptor::WELLKNOWNTYPE_ANY) {
      CEL_ASSIGN_OR_RETURN(
          rhs_unpacked,
          well_known_types::UnpackAnyIfResolveable(
              &arena_, rhs_reflection_.any_reflection, rhs, pool_, factory_));
      if (rhs_unpacked) {
        rhs_ptr = cel::to_address(rhs_unpacked);
        rhs_descriptor = rhs_ptr->GetDescriptor();
        rhs_well_known_type = rhs_descriptor->well_known_type();
      }
    }
    CEL_ASSIGN_OR_RETURN(
        auto lhs_value,
        AsEquatableValue(lhs_reflection_, *lhs_ptr, lhs_descriptor,
                         lhs_well_known_type, lhs_scratch_));
    CEL_ASSIGN_OR_RETURN(
        auto rhs_value,
        AsEquatableValue(rhs_reflection_, *rhs_ptr, rhs_descriptor,
                         rhs_well_known_type, rhs_scratch_));
    return EquatableValueEquals(lhs_value, rhs_value);
  }

  // Equality between map message fields.
  absl::StatusOr<bool> MapFieldEquals(
      const Message& lhs, absl::Nonnull<const FieldDescriptor*> lhs_field,
      const Message& rhs, absl::Nonnull<const FieldDescriptor*> rhs_field) {
    ABSL_DCHECK(lhs_field->is_map());
    ABSL_DCHECK_EQ(lhs_field->containing_type(), lhs.GetDescriptor());
    ABSL_DCHECK(rhs_field->is_map());
    ABSL_DCHECK_EQ(rhs_field->containing_type(), rhs.GetDescriptor());
    const auto* lhs_entry = lhs_field->message_type();
    const auto* lhs_entry_key_field = lhs_entry->map_key();
    const auto* lhs_entry_value_field = lhs_entry->map_value();
    const auto* rhs_entry = rhs_field->message_type();
    const auto* rhs_entry_key_field = rhs_entry->map_key();
    const auto* rhs_entry_value_field = rhs_entry->map_value();
    // Perform cheap test which checks whether the left and right can even be
    // compared for equality.
    if (lhs_field != rhs_field &&
        ((GetEquatableFieldCategory(lhs_entry_key_field) &
          GetEquatableFieldCategory(rhs_entry_key_field)) ==
             EquatableCategory::kNone ||
         (GetEquatableFieldCategory(lhs_entry_value_field) &
          GetEquatableFieldCategory(rhs_entry_value_field)) ==
             EquatableCategory::kNone)) {
      // Short-circuit.
      return false;
    }
    const auto* lhs_reflection = lhs.GetReflection();
    const auto* rhs_reflection = rhs.GetReflection();
    if (MapSize(*lhs_reflection, lhs, *lhs_field) !=
        MapSize(*rhs_reflection, rhs, *rhs_field)) {
      return false;
    }
    auto lhs_begin = MapBegin(*lhs_reflection, lhs, *lhs_field);
    const auto lhs_end = MapEnd(*lhs_reflection, lhs, *lhs_field);
    Unique<Message> lhs_unpacked;
    EquatableValue lhs_value;
    Unique<Message> rhs_unpacked;
    EquatableValue rhs_value;
    google::protobuf::MapKey rhs_map_key;
    google::protobuf::MapValueConstRef rhs_map_value;
    for (; lhs_begin != lhs_end; ++lhs_begin) {
      if (!CoalesceMapKey(lhs_begin.GetKey(), rhs_entry_key_field->cpp_type(),
                          &rhs_map_key)) {
        return false;
      }
      if (!LookupMapValue(*rhs_reflection, rhs, *rhs_field, rhs_map_key,
                          &rhs_map_value)) {
        return false;
      }
      CEL_ASSIGN_OR_RETURN(lhs_value,
                           MapValueAsEquatableValue(
                               &arena_, pool_, factory_, lhs_reflection_,
                               lhs_begin.GetValueRef(), lhs_entry_value_field,
                               lhs_scratch_, lhs_unpacked));
      CEL_ASSIGN_OR_RETURN(
          rhs_value,
          MapValueAsEquatableValue(&arena_, pool_, factory_, rhs_reflection_,
                                   rhs_map_value, rhs_entry_value_field,
                                   rhs_scratch_, rhs_unpacked));
      if (!EquatableValueEquals(lhs_value, rhs_value)) {
        return false;
      }
    }
    return true;
  }

  // Equality between repeated message fields.
  absl::StatusOr<bool> RepeatedFieldEquals(
      const Message& lhs, absl::Nonnull<const FieldDescriptor*> lhs_field,
      const Message& rhs, absl::Nonnull<const FieldDescriptor*> rhs_field) {
    ABSL_DCHECK(lhs_field->is_repeated() && !lhs_field->is_map());
    ABSL_DCHECK_EQ(lhs_field->containing_type(), lhs.GetDescriptor());
    ABSL_DCHECK(rhs_field->is_repeated() && !rhs_field->is_map());
    ABSL_DCHECK_EQ(rhs_field->containing_type(), rhs.GetDescriptor());
    // Perform cheap test which checks whether the left and right can even be
    // compared for equality.
    if (lhs_field != rhs_field &&
        (GetEquatableFieldCategory(lhs_field) &
         GetEquatableFieldCategory(rhs_field)) == EquatableCategory::kNone) {
      // Short-circuit.
      return false;
    }
    const auto* lhs_reflection = lhs.GetReflection();
    const auto* rhs_reflection = rhs.GetReflection();
    const auto size = lhs_reflection->FieldSize(lhs, lhs_field);
    if (size != rhs_reflection->FieldSize(rhs, rhs_field)) {
      return false;
    }
    Unique<Message> lhs_unpacked;
    EquatableValue lhs_value;
    Unique<Message> rhs_unpacked;
    EquatableValue rhs_value;
    for (int i = 0; i < size; ++i) {
      CEL_ASSIGN_OR_RETURN(lhs_value,
                           RepeatedFieldAsEquatableValue(
                               &arena_, pool_, factory_, lhs_reflection_, lhs,
                               lhs_field, i, lhs_scratch_, lhs_unpacked));
      CEL_ASSIGN_OR_RETURN(rhs_value,
                           RepeatedFieldAsEquatableValue(
                               &arena_, pool_, factory_, rhs_reflection_, rhs,
                               rhs_field, i, rhs_scratch_, rhs_unpacked));
      if (!EquatableValueEquals(lhs_value, rhs_value)) {
        return false;
      }
    }
    return true;
  }

  // Equality between singular message fields and/or messages. If the field is
  // `nullptr`, we are performing equality on the message itself rather than the
  // corresponding field.
  absl::StatusOr<bool> SingularFieldEquals(
      const Message& lhs, absl::Nullable<const FieldDescriptor*> lhs_field,
      const Message& rhs, absl::Nullable<const FieldDescriptor*> rhs_field) {
    ABSL_DCHECK(lhs_field == nullptr ||
                (!lhs_field->is_repeated() && !lhs_field->is_map()));
    ABSL_DCHECK(lhs_field == nullptr ||
                lhs_field->containing_type() == lhs.GetDescriptor());
    ABSL_DCHECK(rhs_field == nullptr ||
                (!rhs_field->is_repeated() && !rhs_field->is_map()));
    ABSL_DCHECK(rhs_field == nullptr ||
                rhs_field->containing_type() == rhs.GetDescriptor());
    // Perform cheap test which checks whether the left and right can even be
    // compared for equality.
    if (lhs_field != rhs_field &&
        ((lhs_field != nullptr ? GetEquatableFieldCategory(lhs_field)
                               : GetEquatableCategory(lhs.GetDescriptor())) &
         (rhs_field != nullptr ? GetEquatableFieldCategory(rhs_field)
                               : GetEquatableCategory(rhs.GetDescriptor()))) ==
            EquatableCategory::kNone) {
      // Short-circuit.
      return false;
    }
    absl::Nonnull<const Message*> lhs_ptr = &lhs;
    absl::Nonnull<const Message*> rhs_ptr = &rhs;
    Unique<Message> lhs_unpacked;
    Unique<Message> rhs_unpacked;
    if (lhs_field != nullptr && IsAnyField(lhs_field)) {
      CEL_ASSIGN_OR_RETURN(lhs_unpacked,
                           well_known_types::UnpackAnyIfResolveable(
                               &arena_, lhs_reflection_.any_reflection,
                               lhs.GetReflection()->GetMessage(lhs, lhs_field),
                               pool_, factory_));
      if (lhs_unpacked) {
        lhs_ptr = cel::to_address(lhs_unpacked);
        lhs_field = nullptr;
      }
    } else if (lhs_field == nullptr && IsAny(lhs)) {
      CEL_ASSIGN_OR_RETURN(
          lhs_unpacked,
          well_known_types::UnpackAnyIfResolveable(
              &arena_, lhs_reflection_.any_reflection, lhs, pool_, factory_));
      if (lhs_unpacked) {
        lhs_ptr = cel::to_address(lhs_unpacked);
      }
    }
    if (rhs_field != nullptr && IsAnyField(rhs_field)) {
      CEL_ASSIGN_OR_RETURN(rhs_unpacked,
                           well_known_types::UnpackAnyIfResolveable(
                               &arena_, rhs_reflection_.any_reflection,
                               rhs.GetReflection()->GetMessage(rhs, rhs_field),
                               pool_, factory_));
      if (rhs_unpacked) {
        rhs_ptr = cel::to_address(rhs_unpacked);
        rhs_field = nullptr;
      }
    } else if (rhs_field == nullptr && IsAny(rhs)) {
      CEL_ASSIGN_OR_RETURN(
          rhs_unpacked,
          well_known_types::UnpackAnyIfResolveable(
              &arena_, rhs_reflection_.any_reflection, rhs, pool_, factory_));
      if (rhs_unpacked) {
        rhs_ptr = cel::to_address(rhs_unpacked);
      }
    }
    EquatableValue lhs_value;
    if (lhs_field != nullptr) {
      CEL_ASSIGN_OR_RETURN(
          lhs_value,
          AsEquatableValue(lhs_reflection_, *lhs_ptr, lhs_field, lhs_scratch_));
    } else {
      CEL_ASSIGN_OR_RETURN(
          lhs_value, AsEquatableValue(lhs_reflection_, *lhs_ptr,
                                      lhs_ptr->GetDescriptor(), lhs_scratch_));
    }
    EquatableValue rhs_value;
    if (rhs_field != nullptr) {
      CEL_ASSIGN_OR_RETURN(
          rhs_value,
          AsEquatableValue(rhs_reflection_, *rhs_ptr, rhs_field, rhs_scratch_));
    } else {
      CEL_ASSIGN_OR_RETURN(
          rhs_value, AsEquatableValue(rhs_reflection_, *rhs_ptr,
                                      rhs_ptr->GetDescriptor(), rhs_scratch_));
    }
    return EquatableValueEquals(lhs_value, rhs_value);
  }

  absl::StatusOr<bool> FieldEquals(
      const Message& lhs, absl::Nullable<const FieldDescriptor*> lhs_field,
      const Message& rhs, absl::Nullable<const FieldDescriptor*> rhs_field) {
    ABSL_DCHECK(lhs_field != nullptr ||
                rhs_field != nullptr);  // Both cannot be null.
    if (lhs_field != nullptr && lhs_field->is_map()) {
      // map<?, ?> == map<?, ?>
      // map<?, ?> == google.protobuf.Value
      // map<?, ?> == google.protobuf.Struct
      // map<?, ?> == google.protobuf.Any

      // Right hand side should be a map, `google.protobuf.Value`,
      // `google.protobuf.Struct`, or `google.protobuf.Any`.
      if (rhs_field != nullptr && rhs_field->is_map()) {
        // map<?, ?> == map<?, ?>
        return MapFieldEquals(lhs, lhs_field, rhs, rhs_field);
      }
      if (rhs_field != nullptr &&
          (rhs_field->is_repeated() ||
           rhs_field->type() != FieldDescriptor::TYPE_MESSAGE)) {
        return false;
      }
      absl::Nullable<const Message*> rhs_packed = nullptr;
      Unique<Message> rhs_unpacked;
      if (rhs_field != nullptr && IsAnyField(rhs_field)) {
        rhs_packed = &rhs.GetReflection()->GetMessage(rhs, rhs_field);
      } else if (rhs_field == nullptr && IsAny(rhs)) {
        rhs_packed = &rhs;
      }
      if (rhs_packed != nullptr) {
        CEL_RETURN_IF_ERROR(rhs_reflection_.any_reflection.Initialize(
            rhs_packed->GetDescriptor()));
        auto rhs_type_url = rhs_reflection_.any_reflection.GetTypeUrl(
            *rhs_packed, rhs_scratch_);
        if (!rhs_type_url.ConsumePrefix("type.googleapis.com/") &&
            !rhs_type_url.ConsumePrefix("type.googleprod.com/")) {
          return false;
        }
        if (rhs_type_url != "google.protobuf.Value" &&
            rhs_type_url != "google.protobuf.Struct" &&
            rhs_type_url != "google.protobuf.Any") {
          return false;
        }
        CEL_ASSIGN_OR_RETURN(rhs_unpacked,
                             well_known_types::UnpackAnyIfResolveable(
                                 &arena_, rhs_reflection_.any_reflection,
                                 *rhs_packed, pool_, factory_));
        if (rhs_unpacked) {
          rhs_field = nullptr;
        }
      }
      absl::Nonnull<const Message*> rhs_message =
          rhs_field != nullptr
              ? &rhs.GetReflection()->GetMessage(rhs, rhs_field)
          : rhs_unpacked != nullptr ? cel::to_address(rhs_unpacked)
                                    : &rhs;
      const auto* rhs_descriptor = rhs_message->GetDescriptor();
      const auto rhs_well_known_type = rhs_descriptor->well_known_type();
      switch (rhs_well_known_type) {
        case Descriptor::WELLKNOWNTYPE_VALUE: {
          // map<?, ?> == google.protobuf.Value
          CEL_RETURN_IF_ERROR(
              rhs_reflection_.value_reflection.Initialize(rhs_descriptor));
          if (rhs_reflection_.value_reflection.GetKindCase(*rhs_message) !=
              google::protobuf::Value::kStructValue) {
            return false;
          }
          CEL_RETURN_IF_ERROR(rhs_reflection_.struct_reflection.Initialize(
              rhs_reflection_.value_reflection.GetStructDescriptor()));
          return MapFieldEquals(
              lhs, lhs_field,
              rhs_reflection_.value_reflection.GetStructValue(*rhs_message),
              rhs_reflection_.struct_reflection.GetFieldsDescriptor());
        }
        case Descriptor::WELLKNOWNTYPE_STRUCT: {
          // map<?, ?> == google.protobuf.Struct
          CEL_RETURN_IF_ERROR(
              rhs_reflection_.struct_reflection.Initialize(rhs_descriptor));
          return MapFieldEquals(
              lhs, lhs_field, *rhs_message,
              rhs_reflection_.struct_reflection.GetFieldsDescriptor());
        }
        default:
          return false;
      }
      // Explicitly unreachable, for ease of reading. Control never leaves this
      // if statement.
      ABSL_UNREACHABLE();
    }
    if (rhs_field != nullptr && rhs_field->is_map()) {
      // google.protobuf.Value == map<?, ?>
      // google.protobuf.Struct == map<?, ?>
      // google.protobuf.Any == map<?, ?>

      // Left hand side should be singular `google.protobuf.Value`
      // `google.protobuf.Struct`, or `google.protobuf.Any`.
      ABSL_DCHECK(lhs_field == nullptr ||
                  !lhs_field->is_map());  // Handled above.
      if (lhs_field != nullptr &&
          (lhs_field->is_repeated() ||
           lhs_field->type() != FieldDescriptor::TYPE_MESSAGE)) {
        return false;
      }
      absl::Nullable<const Message*> lhs_packed = nullptr;
      Unique<Message> lhs_unpacked;
      if (lhs_field != nullptr && IsAnyField(lhs_field)) {
        lhs_packed = &lhs.GetReflection()->GetMessage(lhs, lhs_field);
      } else if (lhs_field == nullptr && IsAny(lhs)) {
        lhs_packed = &lhs;
      }
      if (lhs_packed != nullptr) {
        CEL_RETURN_IF_ERROR(lhs_reflection_.any_reflection.Initialize(
            lhs_packed->GetDescriptor()));
        auto lhs_type_url = lhs_reflection_.any_reflection.GetTypeUrl(
            *lhs_packed, lhs_scratch_);
        if (!lhs_type_url.ConsumePrefix("type.googleapis.com/") &&
            !lhs_type_url.ConsumePrefix("type.googleprod.com/")) {
          return false;
        }
        if (lhs_type_url != "google.protobuf.Value" &&
            lhs_type_url != "google.protobuf.Struct" &&
            lhs_type_url != "google.protobuf.Any") {
          return false;
        }
        CEL_ASSIGN_OR_RETURN(lhs_unpacked,
                             well_known_types::UnpackAnyIfResolveable(
                                 &arena_, lhs_reflection_.any_reflection,
                                 *lhs_packed, pool_, factory_));
        if (lhs_unpacked) {
          lhs_field = nullptr;
        }
      }
      absl::Nonnull<const Message*> lhs_message =
          lhs_field != nullptr
              ? &lhs.GetReflection()->GetMessage(lhs, lhs_field)
          : lhs_unpacked != nullptr ? cel::to_address(lhs_unpacked)
                                    : &lhs;
      const auto* lhs_descriptor = lhs_message->GetDescriptor();
      const auto lhs_well_known_type = lhs_descriptor->well_known_type();
      switch (lhs_well_known_type) {
        case Descriptor::WELLKNOWNTYPE_VALUE: {
          // map<?, ?> == google.protobuf.Value
          CEL_RETURN_IF_ERROR(
              lhs_reflection_.value_reflection.Initialize(lhs_descriptor));
          if (lhs_reflection_.value_reflection.GetKindCase(*lhs_message) !=
              google::protobuf::Value::kStructValue) {
            return false;
          }
          CEL_RETURN_IF_ERROR(lhs_reflection_.struct_reflection.Initialize(
              lhs_reflection_.value_reflection.GetStructDescriptor()));
          return MapFieldEquals(
              lhs_reflection_.value_reflection.GetStructValue(*lhs_message),
              lhs_reflection_.struct_reflection.GetFieldsDescriptor(), rhs,
              rhs_field);
        }
        case Descriptor::WELLKNOWNTYPE_STRUCT: {
          // map<?, ?> == google.protobuf.Struct
          CEL_RETURN_IF_ERROR(
              lhs_reflection_.struct_reflection.Initialize(lhs_descriptor));
          return MapFieldEquals(
              *lhs_message,
              lhs_reflection_.struct_reflection.GetFieldsDescriptor(), rhs,
              rhs_field);
        }
        default:
          return false;
      }
      // Explicitly unreachable, for ease of reading. Control never leaves this
      // if statement.
      ABSL_UNREACHABLE();
    }
    ABSL_DCHECK(lhs_field == nullptr ||
                !lhs_field->is_map());  // Handled above.
    ABSL_DCHECK(rhs_field == nullptr ||
                !rhs_field->is_map());  // Handled above.
    if (lhs_field != nullptr && lhs_field->is_repeated()) {
      // repeated<?> == repeated<?>
      // repeated<?> == google.protobuf.Value
      // repeated<?> == google.protobuf.ListValue
      // repeated<?> == google.protobuf.Any

      // Right hand side should be a repeated, `google.protobuf.Value`,
      // `google.protobuf.ListValue`, or `google.protobuf.Any`.
      if (rhs_field != nullptr && rhs_field->is_repeated()) {
        // map<?, ?> == map<?, ?>
        return RepeatedFieldEquals(lhs, lhs_field, rhs, rhs_field);
      }
      if (rhs_field != nullptr &&
          rhs_field->type() != FieldDescriptor::TYPE_MESSAGE) {
        return false;
      }
      absl::Nullable<const Message*> rhs_packed = nullptr;
      Unique<Message> rhs_unpacked;
      if (rhs_field != nullptr && IsAnyField(rhs_field)) {
        rhs_packed = &rhs.GetReflection()->GetMessage(rhs, rhs_field);
      } else if (rhs_field == nullptr && IsAny(rhs)) {
        rhs_packed = &rhs;
      }
      if (rhs_packed != nullptr) {
        CEL_RETURN_IF_ERROR(rhs_reflection_.any_reflection.Initialize(
            rhs_packed->GetDescriptor()));
        auto rhs_type_url = rhs_reflection_.any_reflection.GetTypeUrl(
            *rhs_packed, rhs_scratch_);
        if (!rhs_type_url.ConsumePrefix("type.googleapis.com/") &&
            !rhs_type_url.ConsumePrefix("type.googleprod.com/")) {
          return false;
        }
        if (rhs_type_url != "google.protobuf.Value" &&
            rhs_type_url != "google.protobuf.ListValue" &&
            rhs_type_url != "google.protobuf.Any") {
          return false;
        }
        CEL_ASSIGN_OR_RETURN(rhs_unpacked,
                             well_known_types::UnpackAnyIfResolveable(
                                 &arena_, rhs_reflection_.any_reflection,
                                 *rhs_packed, pool_, factory_));
        if (rhs_unpacked) {
          rhs_field = nullptr;
        }
      }
      absl::Nonnull<const Message*> rhs_message =
          rhs_field != nullptr
              ? &rhs.GetReflection()->GetMessage(rhs, rhs_field)
          : rhs_unpacked != nullptr ? cel::to_address(rhs_unpacked)
                                    : &rhs;
      const auto* rhs_descriptor = rhs_message->GetDescriptor();
      const auto rhs_well_known_type = rhs_descriptor->well_known_type();
      switch (rhs_well_known_type) {
        case Descriptor::WELLKNOWNTYPE_VALUE: {
          // map<?, ?> == google.protobuf.Value
          CEL_RETURN_IF_ERROR(
              rhs_reflection_.value_reflection.Initialize(rhs_descriptor));
          if (rhs_reflection_.value_reflection.GetKindCase(*rhs_message) !=
              google::protobuf::Value::kListValue) {
            return false;
          }
          CEL_RETURN_IF_ERROR(rhs_reflection_.list_value_reflection.Initialize(
              rhs_reflection_.value_reflection.GetListValueDescriptor()));
          return RepeatedFieldEquals(
              lhs, lhs_field,
              rhs_reflection_.value_reflection.GetListValue(*rhs_message),
              rhs_reflection_.list_value_reflection.GetValuesDescriptor());
        }
        case Descriptor::WELLKNOWNTYPE_LISTVALUE: {
          // map<?, ?> == google.protobuf.ListValue
          CEL_RETURN_IF_ERROR(
              rhs_reflection_.list_value_reflection.Initialize(rhs_descriptor));
          return RepeatedFieldEquals(
              lhs, lhs_field, *rhs_message,
              rhs_reflection_.list_value_reflection.GetValuesDescriptor());
        }
        default:
          return false;
      }
      // Explicitly unreachable, for ease of reading. Control never leaves this
      // if statement.
      ABSL_UNREACHABLE();
    }
    if (rhs_field != nullptr && rhs_field->is_repeated()) {
      // google.protobuf.Value == repeated<?>
      // google.protobuf.ListValue == repeated<?>
      // google.protobuf.Any == repeated<?>

      // Left hand side should be singular `google.protobuf.Value`
      // `google.protobuf.ListValue`, or `google.protobuf.Any`.
      ABSL_DCHECK(lhs_field == nullptr ||
                  !lhs_field->is_repeated());  // Handled above.
      if (lhs_field != nullptr &&
          lhs_field->type() != FieldDescriptor::TYPE_MESSAGE) {
        return false;
      }
      absl::Nullable<const Message*> lhs_packed = nullptr;
      Unique<Message> lhs_unpacked;
      if (lhs_field != nullptr && IsAnyField(lhs_field)) {
        lhs_packed = &lhs.GetReflection()->GetMessage(lhs, lhs_field);
      } else if (lhs_field == nullptr && IsAny(lhs)) {
        lhs_packed = &lhs;
      }
      if (lhs_packed != nullptr) {
        CEL_RETURN_IF_ERROR(lhs_reflection_.any_reflection.Initialize(
            lhs_packed->GetDescriptor()));
        auto lhs_type_url = lhs_reflection_.any_reflection.GetTypeUrl(
            *lhs_packed, lhs_scratch_);
        if (!lhs_type_url.ConsumePrefix("type.googleapis.com/") &&
            !lhs_type_url.ConsumePrefix("type.googleprod.com/")) {
          return false;
        }
        if (lhs_type_url != "google.protobuf.Value" &&
            lhs_type_url != "google.protobuf.ListValue" &&
            lhs_type_url != "google.protobuf.Any") {
          return false;
        }
        CEL_ASSIGN_OR_RETURN(lhs_unpacked,
                             well_known_types::UnpackAnyIfResolveable(
                                 &arena_, lhs_reflection_.any_reflection,
                                 *lhs_packed, pool_, factory_));
        if (lhs_unpacked) {
          lhs_field = nullptr;
        }
      }
      absl::Nonnull<const Message*> lhs_message =
          lhs_field != nullptr
              ? &lhs.GetReflection()->GetMessage(lhs, lhs_field)
          : lhs_unpacked != nullptr ? cel::to_address(lhs_unpacked)
                                    : &lhs;
      const auto* lhs_descriptor = lhs_message->GetDescriptor();
      const auto lhs_well_known_type = lhs_descriptor->well_known_type();
      switch (lhs_well_known_type) {
        case Descriptor::WELLKNOWNTYPE_VALUE: {
          // map<?, ?> == google.protobuf.Value
          CEL_RETURN_IF_ERROR(
              lhs_reflection_.value_reflection.Initialize(lhs_descriptor));
          if (lhs_reflection_.value_reflection.GetKindCase(*lhs_message) !=
              google::protobuf::Value::kListValue) {
            return false;
          }
          CEL_RETURN_IF_ERROR(lhs_reflection_.list_value_reflection.Initialize(
              lhs_reflection_.value_reflection.GetListValueDescriptor()));
          return RepeatedFieldEquals(
              lhs_reflection_.value_reflection.GetListValue(*lhs_message),
              lhs_reflection_.list_value_reflection.GetValuesDescriptor(), rhs,
              rhs_field);
        }
        case Descriptor::WELLKNOWNTYPE_LISTVALUE: {
          // map<?, ?> == google.protobuf.ListValue
          CEL_RETURN_IF_ERROR(
              lhs_reflection_.list_value_reflection.Initialize(lhs_descriptor));
          return RepeatedFieldEquals(
              *lhs_message,
              lhs_reflection_.list_value_reflection.GetValuesDescriptor(), rhs,
              rhs_field);
        }
        default:
          return false;
      }
      // Explicitly unreachable, for ease of reading. Control never leaves this
      // if statement.
      ABSL_UNREACHABLE();
    }
    return SingularFieldEquals(lhs, lhs_field, rhs, rhs_field);
  }

 private:
  const absl::Nonnull<const DescriptorPool*> pool_;
  const absl::Nonnull<MessageFactory*> factory_;
  google::protobuf::Arena arena_;
  EquatableValueReflection lhs_reflection_;
  EquatableValueReflection rhs_reflection_;
  std::string lhs_scratch_;
  std::string rhs_scratch_;
};

}  // namespace

absl::StatusOr<bool> MessageEquals(const Message& lhs, const Message& rhs,
                                   absl::Nonnull<const DescriptorPool*> pool,
                                   absl::Nonnull<MessageFactory*> factory) {
  ABSL_DCHECK(pool != nullptr);
  ABSL_DCHECK(factory != nullptr);
  if (&lhs == &rhs) {
    return true;
  }
  // MessageEqualsState has quite a large size, so we allocate it on the heap.
  // Ideally we should just hold most of the state at runtime in something like
  // `FlatExpressionEvaluatorState`, so we can avoid allocating this repeatedly.
  return std::make_unique<MessageEqualsState>(pool, factory)->Equals(lhs, rhs);
}

absl::StatusOr<bool> MessageFieldEquals(
    const Message& lhs, absl::Nonnull<const FieldDescriptor*> lhs_field,
    const Message& rhs, absl::Nonnull<const FieldDescriptor*> rhs_field,
    absl::Nonnull<const DescriptorPool*> pool,
    absl::Nonnull<MessageFactory*> factory) {
  ABSL_DCHECK(lhs_field != nullptr);
  ABSL_DCHECK(rhs_field != nullptr);
  ABSL_DCHECK(pool != nullptr);
  ABSL_DCHECK(factory != nullptr);
  if (&lhs == &rhs && lhs_field == rhs_field) {
    return true;
  }
  // MessageEqualsState has quite a large size, so we allocate it on the heap.
  // Ideally we should just hold most of the state at runtime in something like
  // `FlatExpressionEvaluatorState`, so we can avoid allocating this repeatedly.
  return std::make_unique<MessageEqualsState>(pool, factory)
      ->FieldEquals(lhs, lhs_field, rhs, rhs_field);
}

absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs, const google::protobuf::Message& rhs,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> rhs_field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory) {
  ABSL_DCHECK(rhs_field != nullptr);
  ABSL_DCHECK(pool != nullptr);
  ABSL_DCHECK(factory != nullptr);
  // MessageEqualsState has quite a large size, so we allocate it on the heap.
  // Ideally we should just hold most of the state at runtime in something like
  // `FlatExpressionEvaluatorState`, so we can avoid allocating this repeatedly.
  return std::make_unique<MessageEqualsState>(pool, factory)
      ->FieldEquals(lhs, nullptr, rhs, rhs_field);
}

absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> lhs_field,
    const google::protobuf::Message& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory) {
  ABSL_DCHECK(lhs_field != nullptr);
  ABSL_DCHECK(pool != nullptr);
  ABSL_DCHECK(factory != nullptr);
  // MessageEqualsState has quite a large size, so we allocate it on the heap.
  // Ideally we should just hold most of the state at runtime in something like
  // `FlatExpressionEvaluatorState`, so we can avoid allocating this repeatedly.
  return std::make_unique<MessageEqualsState>(pool, factory)
      ->FieldEquals(lhs, lhs_field, rhs, nullptr);
}

}  // namespace cel::internal
