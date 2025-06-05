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

#include "common/json.h"

#include <initializer_list>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "internal/copy_on_write.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

internal::CopyOnWrite<typename JsonArray::Container> JsonArray::Empty() {
  static const absl::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return *empty;
}

internal::CopyOnWrite<typename JsonObject::Container> JsonObject::Empty() {
  static const absl::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return *empty;
}

Json JsonInt(int64_t value) {
  if (value < kJsonMinInt || value > kJsonMaxInt) {
    return JsonString(absl::StrCat(value));
  }
  return Json(static_cast<double>(value));
}

Json JsonUint(uint64_t value) {
  if (value > kJsonMaxUint) {
    return JsonString(absl::StrCat(value));
  }
  return Json(static_cast<double>(value));
}

Json JsonBytes(absl::string_view value) {
  return JsonString(absl::Base64Escape(value));
}

Json JsonBytes(const absl::Cord& value) {
  if (auto flat = value.TryFlat(); flat.has_value()) {
    return JsonBytes(*flat);
  }
  return JsonBytes(absl::string_view(static_cast<std::string>(value)));
}

bool JsonArrayBuilder::empty() const { return impl_.get().empty(); }

JsonArray JsonArrayBuilder::Build() && { return JsonArray(std::move(impl_)); }

JsonArrayBuilder::JsonArrayBuilder(JsonArray array)
    : impl_(std::move(array.impl_)) {}

JsonObjectBuilder::JsonObjectBuilder(JsonObject object)
    : impl_(std::move(object.impl_)) {}

void JsonObjectBuilder::insert(std::initializer_list<value_type> il) {
  impl_.mutable_get().insert(il);
}

JsonArrayBuilder::size_type JsonArrayBuilder::size() const {
  return impl_.get().size();
}

JsonArrayBuilder::iterator JsonArrayBuilder::begin() {
  return impl_.mutable_get().begin();
}

JsonArrayBuilder::const_iterator JsonArrayBuilder::begin() const {
  return impl_.get().begin();
}

JsonArrayBuilder::iterator JsonArrayBuilder::end() {
  return impl_.mutable_get().end();
}

JsonArrayBuilder::const_iterator JsonArrayBuilder::end() const {
  return impl_.get().end();
}

JsonArrayBuilder::reverse_iterator JsonArrayBuilder::rbegin() {
  return impl_.mutable_get().rbegin();
}

JsonArrayBuilder::reverse_iterator JsonArrayBuilder::rend() {
  return impl_.mutable_get().rend();
}

JsonArrayBuilder::reference JsonArrayBuilder::at(size_type index) {
  return impl_.mutable_get().at(index);
}

JsonArrayBuilder::reference JsonArrayBuilder::operator[](size_type index) {
  return (impl_.mutable_get())[index];
}

void JsonArrayBuilder::reserve(size_type n) {
  if (n != 0) {
    impl_.mutable_get().reserve(n);
  }
}

void JsonArrayBuilder::clear() { impl_.mutable_get().clear(); }

void JsonArrayBuilder::push_back(Json json) {
  impl_.mutable_get().push_back(std::move(json));
}

void JsonArrayBuilder::pop_back() { impl_.mutable_get().pop_back(); }

JsonArrayBuilder::operator JsonArray() && { return std::move(*this).Build(); }

bool JsonArray::empty() const { return impl_.get().empty(); }

JsonArray::JsonArray(internal::CopyOnWrite<Container> impl)
    : impl_(std::move(impl)) {
  if (impl_.get().empty()) {
    impl_ = Empty();
  }
}

JsonArray::size_type JsonArray::size() const { return impl_.get().size(); }

JsonArray::const_iterator JsonArray::begin() const {
  return impl_.get().begin();
}

JsonArray::const_iterator JsonArray::cbegin() const { return begin(); }

JsonArray::const_iterator JsonArray::end() const { return impl_.get().end(); }

JsonArray::const_iterator JsonArray::cend() const { return begin(); }

JsonArray::const_reverse_iterator JsonArray::rbegin() const {
  return impl_.get().rbegin();
}

JsonArray::const_reverse_iterator JsonArray::crbegin() const {
  return impl_.get().crbegin();
}

JsonArray::const_reverse_iterator JsonArray::rend() const {
  return impl_.get().rend();
}

JsonArray::const_reverse_iterator JsonArray::crend() const {
  return impl_.get().crend();
}

JsonArray::const_reference JsonArray::at(size_type index) const {
  return impl_.get().at(index);
}

JsonArray::const_reference JsonArray::operator[](size_type index) const {
  return (impl_.get())[index];
}

bool operator==(const JsonArray& lhs, const JsonArray& rhs) {
  return lhs.impl_.get() == rhs.impl_.get();
}

bool operator!=(const JsonArray& lhs, const JsonArray& rhs) {
  return lhs.impl_.get() != rhs.impl_.get();
}

JsonObjectBuilder::operator JsonObject() && { return std::move(*this).Build(); }

bool JsonObjectBuilder::empty() const { return impl_.get().empty(); }

JsonObjectBuilder::size_type JsonObjectBuilder::size() const {
  return impl_.get().size();
}

JsonObjectBuilder::iterator JsonObjectBuilder::begin() {
  return impl_.mutable_get().begin();
}

JsonObjectBuilder::const_iterator JsonObjectBuilder::begin() const {
  return impl_.get().begin();
}

JsonObjectBuilder::iterator JsonObjectBuilder::end() {
  return impl_.mutable_get().end();
}

JsonObjectBuilder::const_iterator JsonObjectBuilder::end() const {
  return impl_.get().end();
}

void JsonObjectBuilder::clear() { impl_.mutable_get().clear(); }

JsonObject JsonObjectBuilder::Build() && {
  return JsonObject(std::move(impl_));
}

void JsonObjectBuilder::erase(const_iterator pos) {
  impl_.mutable_get().erase(std::move(pos));
}

void JsonObjectBuilder::reserve(size_type n) {
  if (n != 0) {
    impl_.mutable_get().reserve(n);
  }
}

JsonObject MakeJsonObject(
    std::initializer_list<std::pair<JsonString, Json>> il) {
  JsonObjectBuilder builder;
  builder.reserve(il.size());
  for (const auto& entry : il) {
    builder.insert(entry);
  }
  return std::move(builder).Build();
}

JsonObject::JsonObject(internal::CopyOnWrite<Container> impl)
    : impl_(std::move(impl)) {
  if (impl_.get().empty()) {
    impl_ = Empty();
  }
}

bool JsonObject::empty() const { return impl_.get().empty(); }

JsonObject::size_type JsonObject::size() const { return impl_.get().size(); }

JsonObject::const_iterator JsonObject::begin() const {
  return impl_.get().begin();
}

JsonObject::const_iterator JsonObject::cbegin() const { return begin(); }

JsonObject::const_iterator JsonObject::end() const { return impl_.get().end(); }

JsonObject::const_iterator JsonObject::cend() const { return end(); }

bool operator==(const JsonObject& lhs, const JsonObject& rhs) {
  return lhs.impl_.get() == rhs.impl_.get();
}

bool operator!=(const JsonObject& lhs, const JsonObject& rhs) {
  return lhs.impl_.get() != rhs.impl_.get();
}

namespace {

using internal::ProtoWireEncoder;
using internal::ProtoWireTag;
using internal::ProtoWireType;

inline constexpr absl::string_view kJsonTypeName = "google.protobuf.Value";
inline constexpr absl::string_view kJsonArrayTypeName =
    "google.protobuf.ListValue";
inline constexpr absl::string_view kJsonObjectTypeName =
    "google.protobuf.Struct";

inline constexpr ProtoWireTag kValueNullValueFieldTag =
    ProtoWireTag(1, ProtoWireType::kVarint);
inline constexpr ProtoWireTag kValueBoolValueFieldTag =
    ProtoWireTag(4, ProtoWireType::kVarint);
inline constexpr ProtoWireTag kValueNumberValueFieldTag =
    ProtoWireTag(2, ProtoWireType::kFixed64);
inline constexpr ProtoWireTag kValueStringValueFieldTag =
    ProtoWireTag(3, ProtoWireType::kLengthDelimited);
inline constexpr ProtoWireTag kValueListValueFieldTag =
    ProtoWireTag(6, ProtoWireType::kLengthDelimited);
inline constexpr ProtoWireTag kValueStructValueFieldTag =
    ProtoWireTag(5, ProtoWireType::kLengthDelimited);

inline constexpr ProtoWireTag kListValueValuesFieldTag =
    ProtoWireTag(1, ProtoWireType::kLengthDelimited);

inline constexpr ProtoWireTag kStructFieldsEntryKeyFieldTag =
    ProtoWireTag(1, ProtoWireType::kLengthDelimited);
inline constexpr ProtoWireTag kStructFieldsEntryValueFieldTag =
    ProtoWireTag(2, ProtoWireType::kLengthDelimited);

absl::StatusOr<absl::Cord> JsonObjectEntryToAnyValue(const absl::Cord& key,
                                                     const Json& value) {
  absl::Cord data;
  ProtoWireEncoder encoder("google.protobuf.Struct.FieldsEntry", data);
  absl::Cord subdata;
  CEL_RETURN_IF_ERROR(JsonToAnyValue(value, subdata));
  CEL_RETURN_IF_ERROR(encoder.WriteTag(kStructFieldsEntryKeyFieldTag));
  CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(key)));
  CEL_RETURN_IF_ERROR(encoder.WriteTag(kStructFieldsEntryValueFieldTag));
  CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(subdata)));
  encoder.EnsureFullyEncoded();
  return data;
}

inline constexpr ProtoWireTag kStructFieldsFieldTag =
    ProtoWireTag(1, ProtoWireType::kLengthDelimited);

}  // namespace

absl::Status JsonToAnyValue(const Json& json, absl::Cord& data) {
  ProtoWireEncoder encoder(kJsonTypeName, data);
  absl::Status status = absl::visit(
      absl::Overload(
          [&encoder](JsonNull) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueNullValueFieldTag));
            return encoder.WriteVarint(0);
          },
          [&encoder](JsonBool value) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueBoolValueFieldTag));
            return encoder.WriteVarint(value);
          },
          [&encoder](JsonNumber value) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueNumberValueFieldTag));
            return encoder.WriteFixed64(value);
          },
          [&encoder](const JsonString& value) -> absl::Status {
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueStringValueFieldTag));
            return encoder.WriteLengthDelimited(value);
          },
          [&encoder](const JsonArray& value) -> absl::Status {
            absl::Cord subdata;
            CEL_RETURN_IF_ERROR(JsonArrayToAnyValue(value, subdata));
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueListValueFieldTag));
            return encoder.WriteLengthDelimited(std::move(subdata));
          },
          [&encoder](const JsonObject& value) -> absl::Status {
            absl::Cord subdata;
            CEL_RETURN_IF_ERROR(JsonObjectToAnyValue(value, subdata));
            CEL_RETURN_IF_ERROR(encoder.WriteTag(kValueStructValueFieldTag));
            return encoder.WriteLengthDelimited(std::move(subdata));
          }),
      json);
  CEL_RETURN_IF_ERROR(status);
  encoder.EnsureFullyEncoded();
  return absl::OkStatus();
}

absl::Status JsonArrayToAnyValue(const JsonArray& json, absl::Cord& data) {
  ProtoWireEncoder encoder(kJsonArrayTypeName, data);
  for (const auto& element : json) {
    absl::Cord subdata;
    CEL_RETURN_IF_ERROR(JsonToAnyValue(element, subdata));
    CEL_RETURN_IF_ERROR(encoder.WriteTag(kListValueValuesFieldTag));
    CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(subdata)));
  }
  encoder.EnsureFullyEncoded();
  return absl::OkStatus();
}

absl::Status JsonObjectToAnyValue(const JsonObject& json, absl::Cord& data) {
  ProtoWireEncoder encoder(kJsonObjectTypeName, data);
  for (const auto& entry : json) {
    CEL_ASSIGN_OR_RETURN(auto subdata,
                         JsonObjectEntryToAnyValue(entry.first, entry.second));
    CEL_RETURN_IF_ERROR(encoder.WriteTag(kStructFieldsFieldTag));
    CEL_RETURN_IF_ERROR(encoder.WriteLengthDelimited(std::move(subdata)));
  }
  encoder.EnsureFullyEncoded();
  return absl::OkStatus();
}

absl::StatusOr<google::protobuf::Any> JsonToAny(const Json& json) {
  absl::Cord data;
  CEL_RETURN_IF_ERROR(JsonToAnyValue(json, data));
  return MakeAny(MakeTypeUrl(kJsonTypeName), std::move(data));
}

absl::StatusOr<google::protobuf::Any> JsonArrayToAny(const JsonArray& json) {
  absl::Cord data;
  CEL_RETURN_IF_ERROR(JsonArrayToAnyValue(json, data));
  return MakeAny(MakeTypeUrl(kJsonArrayTypeName), std::move(data));
}

absl::StatusOr<google::protobuf::Any> JsonObjectToAny(const JsonObject& json) {
  absl::Cord data;
  CEL_RETURN_IF_ERROR(JsonObjectToAnyValue(json, data));
  return MakeAny(MakeTypeUrl(kJsonObjectTypeName), std::move(data));
}

}  // namespace cel
