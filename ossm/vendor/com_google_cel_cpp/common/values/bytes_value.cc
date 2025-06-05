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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"
#include "internal/strings.h"

namespace cel {

namespace {

template <typename Bytes>
std::string BytesDebugString(const Bytes& value) {
  return value.NativeValue(absl::Overload(
      [](absl::string_view string) -> std::string {
        return internal::FormatBytesLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatBytesLiteral(*flat);
        }
        return internal::FormatBytesLiteral(static_cast<std::string>(cord));
      }));
}

}  // namespace

std::string BytesValue::DebugString() const { return BytesDebugString(*this); }

absl::Status BytesValue::SerializeTo(AnyToJsonConverter&,
                                     absl::Cord& value) const {
  return NativeValue([&value](const auto& bytes) -> absl::Status {
    return internal::SerializeBytesValue(bytes, value);
  });
}

absl::StatusOr<Json> BytesValue::ConvertToJson(AnyToJsonConverter&) const {
  return NativeValue(
      [](const auto& value) -> Json { return JsonBytes(value); });
}

absl::Status BytesValue::Equal(ValueManager&, const Value& other,
                               Value& result) const {
  if (auto other_value = As<BytesValue>(other); other_value.has_value()) {
    result = NativeValue([other_value](const auto& value) -> BoolValue {
      return other_value->NativeValue(
          [&value](const auto& other_value) -> BoolValue {
            return BoolValue{value == other_value};
          });
    });
    return absl::OkStatus();
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

BytesValue BytesValue::Clone(Allocator<> allocator) const {
  return BytesValue(value_.Clone(allocator));
}

size_t BytesValue::Size() const {
  return NativeValue(
      [](const auto& alternative) -> size_t { return alternative.size(); });
}

bool BytesValue::IsEmpty() const {
  return NativeValue(
      [](const auto& alternative) -> bool { return alternative.empty(); });
}

bool BytesValue::Equals(absl::string_view bytes) const {
  return NativeValue([bytes](const auto& alternative) -> bool {
    return alternative == bytes;
  });
}

bool BytesValue::Equals(const absl::Cord& bytes) const {
  return NativeValue([&bytes](const auto& alternative) -> bool {
    return alternative == bytes;
  });
}

bool BytesValue::Equals(const BytesValue& bytes) const {
  return bytes.NativeValue(
      [this](const auto& alternative) -> bool { return Equals(alternative); });
}

namespace {

int CompareImpl(absl::string_view lhs, absl::string_view rhs) {
  return lhs.compare(rhs);
}

int CompareImpl(absl::string_view lhs, const absl::Cord& rhs) {
  return -rhs.Compare(lhs);
}

int CompareImpl(const absl::Cord& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs);
}

int CompareImpl(const absl::Cord& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs);
}

}  // namespace

int BytesValue::Compare(absl::string_view bytes) const {
  return NativeValue([bytes](const auto& alternative) -> int {
    return CompareImpl(alternative, bytes);
  });
}

int BytesValue::Compare(const absl::Cord& bytes) const {
  return NativeValue([&bytes](const auto& alternative) -> int {
    return CompareImpl(alternative, bytes);
  });
}

int BytesValue::Compare(const BytesValue& bytes) const {
  return bytes.NativeValue(
      [this](const auto& alternative) -> int { return Compare(alternative); });
}

}  // namespace cel
