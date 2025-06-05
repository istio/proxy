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

#include "common/value_factory.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/casting.h"
#include "common/internal/arena_string.h"
#include "common/internal/reference_count.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"

namespace cel {

namespace {

void JsonToValue(const Json& json, ValueFactory& value_factory, Value& result) {
  absl::visit(
      absl::Overload(
          [&result](JsonNull) { result = NullValue(); },
          [&result](JsonBool value) { result = BoolValue(value); },
          [&result](JsonNumber value) { result = DoubleValue(value); },
          [&result](const JsonString& value) { result = StringValue(value); },
          [&value_factory, &result](const JsonArray& value) {
            result = value_factory.CreateListValueFromJsonArray(value);
          },
          [&value_factory, &result](const JsonObject& value) {
            result = value_factory.CreateMapValueFromJsonObject(value);
          }),
      json);
}

void JsonDebugString(const Json& json, std::string& out);

void JsonArrayDebugString(const JsonArray& json, std::string& out) {
  out.push_back('[');
  auto element = json.begin();
  if (element != json.end()) {
    JsonDebugString(*element, out);
    ++element;
    for (; element != json.end(); ++element) {
      out.append(", ");
      JsonDebugString(*element, out);
    }
  }
  out.push_back(']');
}

void JsonObjectEntryDebugString(const JsonString& key, const Json& value,
                                std::string& out) {
  out.append(StringValue(key).DebugString());
  out.append(": ");
  JsonDebugString(value, out);
}

void JsonObjectDebugString(const JsonObject& json, std::string& out) {
  std::vector<JsonString> keys;
  keys.reserve(json.size());
  for (const auto& entry : json) {
    keys.push_back(entry.first);
  }
  std::stable_sort(keys.begin(), keys.end());
  out.push_back('{');
  auto key = keys.begin();
  if (key != keys.end()) {
    JsonObjectEntryDebugString(*key, json.find(*key)->second, out);
    ++key;
    for (; key != keys.end(); ++key) {
      out.append(", ");
      JsonObjectEntryDebugString(*key, json.find(*key)->second, out);
    }
  }
  out.push_back('}');
}

void JsonDebugString(const Json& json, std::string& out) {
  absl::visit(
      absl::Overload(
          [&out](JsonNull) -> void { out.append(NullValue().DebugString()); },
          [&out](JsonBool value) -> void {
            out.append(BoolValue(value).DebugString());
          },
          [&out](JsonNumber value) -> void {
            out.append(DoubleValue(value).DebugString());
          },
          [&out](const JsonString& value) -> void {
            out.append(StringValue(value).DebugString());
          },
          [&out](const JsonArray& value) -> void {
            JsonArrayDebugString(value, out);
          },
          [&out](const JsonObject& value) -> void {
            JsonObjectDebugString(value, out);
          }),
      json);
}

class JsonListValue final : public ParsedListValueInterface {
 public:
  explicit JsonListValue(JsonArray array) : array_(std::move(array)) {}

  std::string DebugString() const override {
    std::string out;
    JsonArrayDebugString(array_, out);
    return out;
  }

  bool IsEmpty() const override { return array_.empty(); }

  size_t Size() const override { return array_.size(); }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter&) const override {
    return array_;
  }

  ParsedListValue Clone(ArenaAllocator<> allocator) const override {
    return ParsedListValue(MemoryManager::Pooling(allocator.arena())
                               .MakeShared<JsonListValue>(array_));
  }

 private:
  absl::Status GetImpl(ValueManager& value_manager, size_t index,
                       Value& result) const override {
    JsonToValue(array_[index], value_manager, result);
    return absl::OkStatus();
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<JsonListValue>();
  }

  const JsonArray array_;
};

class JsonMapValueKeyIterator final : public ValueIterator {
 public:
  explicit JsonMapValueKeyIterator(
      const JsonObject& object ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : begin_(object.begin()), end_(object.end()) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(ValueManager&, Value& result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    const auto& key = begin_->first;
    ++begin_;
    result = StringValue(key);
    return absl::OkStatus();
  }

 private:
  typename JsonObject::const_iterator begin_;
  typename JsonObject::const_iterator end_;
};

class JsonMapValue final : public ParsedMapValueInterface {
 public:
  explicit JsonMapValue(JsonObject object) : object_(std::move(object)) {}

  std::string DebugString() const override {
    std::string out;
    JsonObjectDebugString(object_, out);
    return out;
  }

  bool IsEmpty() const override { return object_.empty(); }

  size_t Size() const override { return object_.size(); }

  // Returns a new list value whose elements are the keys of this map.
  absl::Status ListKeys(ValueManager& value_manager,
                        ListValue& result) const override {
    JsonArrayBuilder keys;
    keys.reserve(object_.size());
    for (const auto& entry : object_) {
      keys.push_back(entry.first);
    }
    result = ParsedListValue(
        value_manager.GetMemoryManager().MakeShared<JsonListValue>(
            std::move(keys).Build()));
    return absl::OkStatus();
  }

  // By default, implementations do not guarantee any iteration order. Unless
  // specified otherwise, assume the iteration order is random.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager&) const override {
    return std::make_unique<JsonMapValueKeyIterator>(object_);
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter&) const override {
    return object_;
  }

  ParsedMapValue Clone(ArenaAllocator<> allocator) const override {
    return ParsedMapValue(MemoryManager::Pooling(allocator.arena())
                              .MakeShared<JsonMapValue>(object_));
  }

 private:
  // Called by `Find` after performing various argument checks.
  absl::StatusOr<bool> FindImpl(ValueManager& value_manager, const Value& key,
                                Value& result) const override {
    return Cast<StringValue>(key).NativeValue(absl::Overload(
        [this, &value_manager, &result](absl::string_view value) -> bool {
          if (auto entry = object_.find(value); entry != object_.end()) {
            JsonToValue(entry->second, value_manager, result);
            return true;
          }
          return false;
        },
        [this, &value_manager, &result](const absl::Cord& value) -> bool {
          if (auto entry = object_.find(value); entry != object_.end()) {
            JsonToValue(entry->second, value_manager, result);
            return true;
          }
          return false;
        }));
  }

  // Called by `Has` after performing various argument checks.
  absl::StatusOr<bool> HasImpl(ValueManager&, const Value& key) const override {
    return Cast<StringValue>(key).NativeValue(absl::Overload(
        [this](absl::string_view value) -> bool {
          return object_.contains(value);
        },
        [this](const absl::Cord& value) -> bool {
          return object_.contains(value);
        }));
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<JsonMapValue>();
  }

  const JsonObject object_;
};

}  // namespace

Value ValueFactory::CreateValueFromJson(Json json) {
  return absl::visit(
      absl::Overload(
          [](JsonNull) -> Value { return NullValue(); },
          [](JsonBool value) -> Value { return BoolValue(value); },
          [](JsonNumber value) -> Value { return DoubleValue(value); },
          [](const JsonString& value) -> Value { return StringValue(value); },
          [this](JsonArray value) -> Value {
            return CreateListValueFromJsonArray(std::move(value));
          },
          [this](JsonObject value) -> Value {
            return CreateMapValueFromJsonObject(std::move(value));
          }),
      std::move(json));
}

ListValue ValueFactory::CreateListValueFromJsonArray(JsonArray json) {
  if (json.empty()) {
    return ListValue(GetZeroDynListValue());
  }
  return ParsedListValue(
      GetMemoryManager().MakeShared<JsonListValue>(std::move(json)));
}

MapValue ValueFactory::CreateMapValueFromJsonObject(JsonObject json) {
  if (json.empty()) {
    return MapValue(GetZeroStringDynMapValue());
  }
  return ParsedMapValue(
      GetMemoryManager().MakeShared<JsonMapValue>(std::move(json)));
}

ListValue ValueFactory::GetZeroDynListValue() { return ListValue(); }

MapValue ValueFactory::GetZeroDynDynMapValue() { return MapValue(); }

MapValue ValueFactory::GetZeroStringDynMapValue() { return MapValue(); }

OptionalValue ValueFactory::GetZeroDynOptionalValue() {
  return OptionalValue();
}

namespace {

class ReferenceCountedString final : public common_internal::ReferenceCounted {
 public:
  static const ReferenceCountedString* New(std::string&& string) {
    return new ReferenceCountedString(std::move(string));
  }

  const char* data() const {
    return std::launder(reinterpret_cast<const std::string*>(&string_[0]))
        ->data();
  }

  size_t size() const {
    return std::launder(reinterpret_cast<const std::string*>(&string_[0]))
        ->size();
  }

 private:
  explicit ReferenceCountedString(std::string&& robbed) : ReferenceCounted() {
    ::new (static_cast<void*>(&string_[0])) std::string(std::move(robbed));
  }

  void Finalize() noexcept override {
    std::launder(reinterpret_cast<const std::string*>(&string_[0]))
        ->~basic_string();
  }

  alignas(std::string) char string_[sizeof(std::string)];
};

}  // namespace

static void StringDestructor(void* string) {
  static_cast<std::string*>(string)->~basic_string();
}

absl::StatusOr<BytesValue> ValueFactory::CreateBytesValue(std::string value) {
  auto memory_manager = GetMemoryManager();
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling: {
      auto* string = ::new (
          memory_manager.Allocate(sizeof(std::string), alignof(std::string)))
          std::string(std::move(value));
      memory_manager.OwnCustomDestructor(string, &StringDestructor);
      return BytesValue{common_internal::ArenaString(*string)};
    }
    case MemoryManagement::kReferenceCounting: {
      auto* refcount = ReferenceCountedString::New(std::move(value));
      auto bytes_value = BytesValue{common_internal::SharedByteString(
          refcount, absl::string_view(refcount->data(), refcount->size()))};
      common_internal::StrongUnref(*refcount);
      return bytes_value;
    }
  }
}

StringValue ValueFactory::CreateUncheckedStringValue(std::string value) {
  auto memory_manager = GetMemoryManager();
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling: {
      auto* string = ::new (
          memory_manager.Allocate(sizeof(std::string), alignof(std::string)))
          std::string(std::move(value));
      memory_manager.OwnCustomDestructor(string, &StringDestructor);
      return StringValue{common_internal::ArenaString(*string)};
    }
    case MemoryManagement::kReferenceCounting: {
      auto* refcount = ReferenceCountedString::New(std::move(value));
      auto string_value = StringValue{common_internal::SharedByteString(
          refcount, absl::string_view(refcount->data(), refcount->size()))};
      common_internal::StrongUnref(*refcount);
      return string_value;
    }
  }
}

absl::StatusOr<StringValue> ValueFactory::CreateStringValue(std::string value) {
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return CreateUncheckedStringValue(std::move(value));
}

absl::StatusOr<StringValue> ValueFactory::CreateStringValue(absl::Cord value) {
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return StringValue(std::move(value));
}

absl::StatusOr<DurationValue> ValueFactory::CreateDurationValue(
    absl::Duration value) {
  CEL_RETURN_IF_ERROR(internal::ValidateDuration(value));
  return DurationValue{value};
}

absl::StatusOr<TimestampValue> ValueFactory::CreateTimestampValue(
    absl::Time value) {
  CEL_RETURN_IF_ERROR(internal::ValidateTimestamp(value));
  return TimestampValue{value};
}

}  // namespace cel
