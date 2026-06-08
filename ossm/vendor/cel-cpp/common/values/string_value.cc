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
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_buffer.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/internal/byte_string.h"
#include "common/internal/reference_count.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/utf8.h"
#include "internal/well_known_types.h"
#include "runtime/internal/errors.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::ValueReflection;

template <typename Bytes>
std::string StringDebugString(const Bytes& value) {
  return value.NativeValue(absl::Overload(
      [](absl::string_view string) -> std::string {
        return internal::FormatStringLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatStringLiteral(*flat);
        }
        return internal::FormatStringLiteral(static_cast<std::string>(cord));
      }));
}

}  // namespace

StringValue StringValue::Concat(const StringValue& lhs, const StringValue& rhs,
                                google::protobuf::Arena* absl_nonnull arena) {
  return StringValue(
      common_internal::ByteString::Concat(lhs.value_, rhs.value_, arena));
}

std::string StringValue::DebugString() const {
  return StringDebugString(*this);
}

absl::Status StringValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  google::protobuf::StringValue message;
  message.set_value(NativeString());
  if (!message.SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", message.GetTypeName()));
  }

  return absl::OkStatus();
}

absl::Status StringValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  ValueReflection value_reflection;
  CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
  NativeValue(
      [&](const auto& value) { value_reflection.SetStringValue(json, value); });

  return absl::OkStatus();
}

absl::Status StringValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_value = other.AsString(); other_value.has_value()) {
    *result = NativeValue([other_value](const auto& value) -> BoolValue {
      return other_value->NativeValue(
          [&value](const auto& other_value) -> BoolValue {
            return BoolValue{value == other_value};
          });
    });
    return absl::OkStatus();
  }
  *result = FalseValue();
  return absl::OkStatus();
}

size_t StringValue::Size() const {
  return NativeValue([](const auto& alternative) -> size_t {
    return internal::Utf8CodePointCount(alternative);
  });
}

bool StringValue::IsEmpty() const {
  return NativeValue(
      [](const auto& alternative) -> bool { return alternative.empty(); });
}

bool StringValue::Equals(absl::string_view string) const {
  return value_.Equals(string);
}

bool StringValue::Equals(const absl::Cord& string) const {
  return value_.Equals(string);
}

bool StringValue::Equals(const StringValue& string) const {
  return value_.Equals(string.value_);
}

StringValue StringValue::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  return StringValue(value_.Clone(arena));
}

int StringValue::Compare(absl::string_view string) const {
  return value_.Compare(string);
}

int StringValue::Compare(const absl::Cord& string) const {
  return value_.Compare(string);
}

int StringValue::Compare(const StringValue& string) const {
  return value_.Compare(string.value_);
}

bool StringValue::StartsWith(absl::string_view string) const {
  return value_.StartsWith(string);
}

bool StringValue::StartsWith(const absl::Cord& string) const {
  return value_.StartsWith(string);
}

bool StringValue::StartsWith(const StringValue& string) const {
  return value_.StartsWith(string.value_);
}

bool StringValue::EndsWith(absl::string_view string) const {
  return value_.EndsWith(string);
}

bool StringValue::EndsWith(const absl::Cord& string) const {
  return value_.EndsWith(string);
}

bool StringValue::EndsWith(const StringValue& string) const {
  return value_.EndsWith(string.value_);
}

bool StringValue::Contains(absl::string_view string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> bool {
        return absl::StrContains(lhs, string);
      },
      [&](const absl::Cord& lhs) -> bool { return lhs.Contains(string); }));
}

bool StringValue::Contains(const absl::Cord& string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> bool {
        if (auto flat = string.TryFlat(); flat) {
          return absl::StrContains(lhs, *flat);
        }
        // There is no nice way to do this. We cannot use std::search due to
        // absl::Cord::CharIterator being an input iterator instead of a forward
        // iterator. So just make an external cord with a noop releaser. We know
        // the external cord will not outlive this function.
        return absl::MakeCordFromExternal(lhs, []() {}).Contains(string);
      },
      [&](const absl::Cord& lhs) -> bool { return lhs.Contains(string); }));
}

bool StringValue::Contains(const StringValue& string) const {
  return string.value_.Visit(absl::Overload(
      [&](absl::string_view rhs) -> bool { return Contains(rhs); },
      [&](const absl::Cord& rhs) -> bool { return Contains(rhs); }));
}

absl::optional<int64_t> StringValue::IndexOf(absl::string_view string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (absl::StartsWith(lhs, string)) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.StartsWith(string)) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      }));
}

absl::optional<int64_t> StringValue::IndexOf(const absl::Cord& string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.substr(0, string.size()) == string) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.StartsWith(string)) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      }));
}

absl::optional<int64_t> StringValue::IndexOf(const StringValue& string) const {
  return string.value_.Visit(absl::Overload(
      [this](absl::string_view rhs) -> absl::optional<int64_t> {
        return IndexOf(rhs);
      },
      [this](const absl::Cord& rhs) -> absl::optional<int64_t> {
        return IndexOf(rhs);
      }));
}

absl::optional<int64_t> StringValue::IndexOf(absl::string_view string,
                                             int64_t pos) const {
  if (pos < 0) {
    pos = 0;
  }
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (code_points >= pos && absl::StartsWith(lhs, string)) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (code_points >= pos && lhs.StartsWith(string)) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      }));
}

absl::optional<int64_t> StringValue::IndexOf(const absl::Cord& string,
                                             int64_t pos) const {
  if (pos < 0) {
    pos = 0;
  }
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (code_points >= pos && lhs.substr(0, string.size()) == string) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (code_points >= pos && lhs.StartsWith(string)) {
            return code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        return absl::nullopt;
      }));
}

absl::optional<int64_t> StringValue::IndexOf(const StringValue& string,
                                             int64_t pos) const {
  return string.value_.Visit(absl::Overload(
      [this, pos](absl::string_view rhs) -> absl::optional<int64_t> {
        return IndexOf(rhs, pos);
      },
      [this, pos](const absl::Cord& rhs) -> absl::optional<int64_t> {
        return IndexOf(rhs, pos);
      }));
}

absl::optional<int64_t> StringValue::LastIndexOf(
    absl::string_view string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (absl::StartsWith(lhs, string)) {
            last_index = code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.StartsWith(string)) {
            last_index = code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      }));
}

absl::optional<int64_t> StringValue::LastIndexOf(
    const absl::Cord& string) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.substr(0, string.size()) == string) {
            last_index = code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.StartsWith(string)) {
            last_index = code_points;
          }
          if (lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      }));
}

absl::optional<int64_t> StringValue::LastIndexOf(
    const StringValue& string) const {
  return string.value_.Visit(absl::Overload(
      [this](absl::string_view rhs) -> absl::optional<int64_t> {
        return LastIndexOf(rhs);
      },
      [this](const absl::Cord& rhs) -> absl::optional<int64_t> {
        return LastIndexOf(rhs);
      }));
}

absl::optional<int64_t> StringValue::LastIndexOf(absl::string_view string,
                                                 int64_t pos) const {
  if (pos < 0) {
    return absl::nullopt;
  }
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (absl::StartsWith(lhs, string)) {
            last_index = code_points;
          }
          if (code_points >= pos || lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.StartsWith(string)) {
            last_index = code_points;
          }
          if (code_points >= pos || lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      }));
}

absl::optional<int64_t> StringValue::LastIndexOf(const absl::Cord& string,
                                                 int64_t pos) const {
  if (pos < 0) {
    return absl::nullopt;
  }
  return value_.Visit(absl::Overload(
      [&](absl::string_view lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.substr(0, string.size()) == string) {
            last_index = code_points;
          }
          if (code_points >= pos || lhs.size() == string.size()) {
            break;
          }
          size_t code_units =
              cel::internal::Utf8Decode(lhs, /*code_point=*/nullptr);
          lhs.remove_prefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      },
      [&](absl::Cord lhs) -> absl::optional<int64_t> {
        int64_t last_index = -1;
        int64_t code_points = 0;
        while (lhs.size() >= string.size()) {
          if (lhs.StartsWith(string)) {
            last_index = code_points;
          }
          if (code_points >= pos || lhs.size() == string.size()) {
            break;
          }
          size_t code_units = cel::internal::Utf8Decode(lhs.char_begin(),
                                                        /*code_point=*/nullptr);
          lhs.RemovePrefix(code_units);
          ++code_points;
        }
        if (last_index < 0) return absl::nullopt;
        return last_index;
      }));
}

absl::optional<int64_t> StringValue::LastIndexOf(const StringValue& string,
                                                 int64_t pos) const {
  return string.value_.Visit(absl::Overload(
      [this, pos](absl::string_view rhs) -> absl::optional<int64_t> {
        return LastIndexOf(rhs, pos);
      },
      [this, pos](const absl::Cord& rhs) -> absl::optional<int64_t> {
        return LastIndexOf(rhs, pos);
      }));
}

namespace {

absl::StatusOr<size_t> SubstringImpl(absl::string_view string, uint64_t start) {
  size_t size_code_points = 0;
  size_t size_code_units = 0;
  while (!string.empty()) {
    char32_t code_point;
    size_t code_units;
    std::tie(code_point, code_units) = cel::internal::Utf8Decode(string);
    if (size_code_points == start) {
      return size_code_units;
    }
    string.remove_prefix(code_units);
    ++size_code_points;
    size_code_units += code_units;
  }
  if (size_code_points == start) {
    return size_code_units;
  }
  return absl::InvalidArgumentError(
      "<string>.substring(<start>): <start> is greater than <string>.size()");
}

absl::StatusOr<absl::Cord> SubstringImpl(const absl::Cord& cord,
                                         uint64_t start) {
  absl::Cord::CharIterator char_begin = cord.char_begin();
  absl::Cord::CharIterator char_end = cord.char_end();
  size_t size_code_points = 0;
  size_t size_code_units = 0;
  while (char_begin != char_end) {
    char32_t code_point;
    size_t code_units;
    std::tie(code_point, code_units) = cel::internal::Utf8Decode(char_begin);
    if (size_code_points == start) {
      return cord.Subcord(size_code_units, std::numeric_limits<size_t>::max());
    }
    absl::Cord::Advance(&char_begin, code_units);
    ++size_code_points;
    size_code_units += code_units;
  }
  if (size_code_points == start) {
    return cord;
  }
  return absl::InvalidArgumentError(
      "<string>.substring(<start>): <start> is greater than <string>.size()");
}

}  // namespace

Value StringValue::Substring(int64_t start) const {
  if (start < 0) {
    return ErrorValue(absl::InvalidArgumentError(
        "<string>.substring(<start>): <start> is less than 0"));
  }
  if (static_cast<uint64_t>(start) > value_.size()) {
    return ErrorValue(absl::InvalidArgumentError(
        "<string>.substring(<start>, <end>): <start> or <end> is greater than "
        "<string>.size()"));
  }
  if (start == 0) {
    return *this;
  }
  switch (value_.GetKind()) {
    case common_internal::ByteStringKind::kSmall: {
      absl::StatusOr<size_t> status_or_index =
          (SubstringImpl)(value_.GetSmall(), start);
      if (!status_or_index.ok()) {
        return ErrorValue(std::move(status_or_index).status());
      }
      StringValue result;
      result.value_.rep_.header.kind = common_internal::ByteStringKind::kSmall;
      result.value_.rep_.small.size = value_.rep_.small.size - *status_or_index;
      std::memcpy(result.value_.rep_.small.data,
                  value_.rep_.small.data + *status_or_index,
                  result.value_.rep_.small.size);
      result.value_.rep_.small.arena = value_.rep_.small.arena;
      return result;
    }
    case common_internal::ByteStringKind::kMedium: {
      absl::StatusOr<size_t> status_or_index =
          (SubstringImpl)(value_.GetMedium(), start);
      if (!status_or_index.ok()) {
        return ErrorValue(std::move(status_or_index).status());
      }
      StringValue result;
      result.value_.rep_.header.kind = common_internal::ByteStringKind::kMedium;
      result.value_.rep_.medium.size =
          value_.rep_.medium.size - *status_or_index;
      result.value_.rep_.medium.data =
          value_.rep_.medium.data + *status_or_index;
      result.value_.rep_.medium.owner = value_.rep_.medium.owner;
      common_internal::StrongRef(result.value_.GetMediumReferenceCount());
      return result;
    }
    case common_internal::ByteStringKind::kLarge: {
      absl::StatusOr<absl::Cord> status_or_cord =
          (SubstringImpl)(value_.GetLarge(), start);
      if (!status_or_cord.ok()) {
        return ErrorValue(std::move(status_or_cord).status());
      }
      return StringValue::Wrap(*std::move(status_or_cord));
    }
  }
}

namespace {

absl::StatusOr<std::pair<size_t, size_t>> SubstringImpl(
    absl::string_view string, uint64_t start, uint64_t end) {
  size_t size_code_points = 0;
  size_t size_code_units = 0;
  size_t start_code_units;
  while (!string.empty()) {
    if (size_code_points == start) {
      start_code_units = size_code_units;
    }
    if (size_code_points == end) {
      return std::pair{start_code_units, size_code_units};
    }
    char32_t code_point;
    size_t code_units;
    std::tie(code_point, code_units) = cel::internal::Utf8Decode(string);
    string.remove_prefix(code_units);
    ++size_code_points;
    size_code_units += code_units;
  }
  if (size_code_points == start && start == end) {
    return std::pair{size_code_units, size_code_units};
  }
  return absl::InvalidArgumentError(
      "<string>.substring(<start>, <end>): <start> or <end> is greater than "
      "<string>.size()");
}

absl::StatusOr<absl::Cord> SubstringImpl(const absl::Cord& cord, uint64_t start,
                                         uint64_t end) {
  absl::Cord::CharIterator char_begin = cord.char_begin();
  absl::Cord::CharIterator char_end = cord.char_end();
  size_t size_code_points = 0;
  size_t size_code_units = 0;
  size_t start_code_units;
  while (char_begin != char_end) {
    if (size_code_points == start) {
      start_code_units = size_code_units;
    }
    if (size_code_points == end) {
      return cord.Subcord(start_code_units,
                          size_code_points - start_code_units);
    }
    char32_t code_point;
    size_t code_units;
    std::tie(code_point, code_units) = cel::internal::Utf8Decode(char_begin);
    absl::Cord::Advance(&char_begin, code_units);
    ++size_code_points;
    size_code_units += code_units;
  }
  if (size_code_points == start && start == end) {
    return absl::Cord();
  }
  return absl::InvalidArgumentError(
      "<string>.substring(<start>, <end>): <start> or <end> is greater than "
      "<string>.size()");
}

}  // namespace

Value StringValue::Substring(int64_t start, int64_t end) const {
  if (start < 0) {
    return ErrorValue(absl::InvalidArgumentError(
        "<string>.substring(<start>, <end>): <start> is less than 0"));
  }
  if (end < start) {
    return ErrorValue(absl::InvalidArgumentError(
        "<string>.substring(<start>, <end>): <end> is less than <start>"));
  }
  if (static_cast<uint64_t>(start) > value_.size() ||
      static_cast<uint64_t>(end) > value_.size()) {
    return ErrorValue(absl::InvalidArgumentError(
        "<string>.substring(<start>, <end>): <start> or <end> is greater than "
        "<string>.size()"));
  }
  switch (value_.GetKind()) {
    case common_internal::ByteStringKind::kSmall: {
      absl::StatusOr<std::pair<size_t, size_t>> status_or_indices =
          (SubstringImpl)(value_.GetSmall(), start, end);
      if (!status_or_indices.ok()) {
        return ErrorValue(std::move(status_or_indices).status());
      }
      StringValue result;
      result.value_.rep_.header.kind = common_internal::ByteStringKind::kSmall;
      result.value_.rep_.small.size =
          (status_or_indices->second - status_or_indices->first);
      std::memcpy(result.value_.rep_.small.data,
                  value_.rep_.small.data + status_or_indices->first,
                  result.value_.rep_.small.size);
      result.value_.rep_.small.arena = value_.rep_.small.arena;
      return result;
    }
    case common_internal::ByteStringKind::kMedium: {
      absl::StatusOr<std::pair<size_t, size_t>> status_or_indices =
          (SubstringImpl)(value_.GetMedium(), start, end);
      if (!status_or_indices.ok()) {
        return ErrorValue(std::move(status_or_indices).status());
      }
      StringValue result;
      result.value_.rep_.header.kind = common_internal::ByteStringKind::kMedium;
      result.value_.rep_.medium.size =
          (status_or_indices->second - status_or_indices->first);
      result.value_.rep_.medium.data =
          value_.rep_.medium.data + status_or_indices->first;
      result.value_.rep_.medium.owner = value_.rep_.medium.owner;
      common_internal::StrongRef(result.value_.GetMediumReferenceCount());
      return result;
    }
    case common_internal::ByteStringKind::kLarge: {
      absl::StatusOr<absl::Cord> status_or_cord =
          (SubstringImpl)(value_.GetLarge(), start, end);
      if (!status_or_cord.ok()) {
        return ErrorValue(std::move(status_or_cord).status());
      }
      return StringValue::Wrap(*std::move(status_or_cord));
    }
  }
}

namespace {

bool LowerAsciiImpl(absl::string_view in, std::string* absl_nonnull out) {
  if (in.empty()) {
    return false;
  }
  bool needs_conversion = false;
  for (char c : in) {
    if (absl::ascii_isupper(c)) {
      needs_conversion = true;
      break;
    }
  }

  if (!needs_conversion) {
    return false;
  }

  *out = absl::AsciiStrToLower(in);
  return true;
}

absl::Cord LowerAsciiImpl(const absl::Cord& in) {
  if (in.empty()) {
    return in;
  }
  size_t pos = 0;
  bool needs_conversion = false;
  for (char c : in.Chars()) {
    if (absl::ascii_isupper(c)) {
      needs_conversion = true;
      break;
    }
    pos++;
  }
  if (!needs_conversion) {
    return in;
  }
  absl::Cord out = in.Subcord(0, pos);
  absl::Cord rest = in.Subcord(pos, in.size() - pos);
  std::string suffix;
  suffix.resize(rest.size());
  size_t current = 0;
  for (char c : rest.Chars()) {
    suffix[current++] = absl::ascii_tolower(c);
  }
  out.Append(std::move(suffix));
  return out;
}

}  // namespace

StringValue StringValue::LowerAscii(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  switch (value_.GetKind()) {
    case common_internal::ByteStringKind::kSmall: {
      std::string out;
      if (!(LowerAsciiImpl)(value_.GetSmall(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kMedium: {
      std::string out;
      if (!(LowerAsciiImpl)(value_.GetMedium(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kLarge:
      return StringValue::Wrap((LowerAsciiImpl)(value_.GetLarge()));
  }
}

namespace {

bool UpperAsciiImpl(absl::string_view in, std::string* absl_nonnull out) {
  if (in.empty()) {
    return false;
  }
  bool needs_conversion = false;
  for (char c : in) {
    if (absl::ascii_islower(c)) {
      needs_conversion = true;
      break;
    }
  }

  if (!needs_conversion) {
    return false;
  }

  *out = absl::AsciiStrToUpper(in);
  return true;
}

absl::Cord UpperAsciiImpl(const absl::Cord& in) {
  if (in.empty()) {
    return in;
  }
  size_t pos = 0;
  bool needs_conversion = false;
  for (char c : in.Chars()) {
    if (absl::ascii_islower(c)) {
      needs_conversion = true;
      break;
    }
    pos++;
  }
  if (!needs_conversion) {
    return in;
  }
  absl::Cord out = in.Subcord(0, pos);
  absl::Cord rest = in.Subcord(pos, in.size() - pos);
  std::string suffix;
  suffix.resize(rest.size());
  size_t current = 0;
  for (char c : rest.Chars()) {
    suffix[current++] = absl::ascii_toupper(c);
  }
  out.Append(std::move(suffix));
  return out;
}

}  // namespace

StringValue StringValue::UpperAscii(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  switch (value_.GetKind()) {
    case common_internal::ByteStringKind::kSmall: {
      std::string out;
      if (!(UpperAsciiImpl)(value_.GetSmall(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kMedium: {
      std::string out;
      if (!(UpperAsciiImpl)(value_.GetMedium(), &out)) {
        return *this;
      }
      return StringValue::From(std::move(out), arena);
    }
    case common_internal::ByteStringKind::kLarge:
      return StringValue::Wrap((UpperAsciiImpl)(value_.GetLarge()));
  }
}

namespace {

// Per CEL spec, checking for Unicode whitespace.
bool IsUnicodeWhitespace(char32_t c) {
  if (c <= 0x0020) {
    return c == 0x0020 || (c >= 0x0009 && c <= 0x000D);
  }
  if (c > 0x3000) return false;
  if (c == 0x0085 || c == 0x00a0 || c == 0x1680) return true;
  if (c >= 0x2000 && c <= 0x200a) return true;
  return c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
         c == 0x3000;
}

std::pair<size_t, size_t> TrimImpl(absl::string_view string) {
  absl::string_view temp_string = string;
  size_t left_trim_bytes = 0;
  while (!temp_string.empty()) {
    char32_t c;
    size_t char_len = cel::internal::Utf8Decode(temp_string, &c);
    if (!IsUnicodeWhitespace(c)) {
      break;
    }
    temp_string.remove_prefix(char_len);
    left_trim_bytes += char_len;
  }

  if (left_trim_bytes == string.size()) {
    return {left_trim_bytes, 0};
  }

  size_t last_non_ws_end_bytes = 0;
  size_t current_pos_bytes = 0;
  temp_string = string;
  while (!temp_string.empty()) {
    char32_t c;
    size_t char_len = cel::internal::Utf8Decode(temp_string, &c);
    if (!IsUnicodeWhitespace(c)) {
      last_non_ws_end_bytes = current_pos_bytes + char_len;
    }
    current_pos_bytes += char_len;
    temp_string.remove_prefix(char_len);
  }

  return {left_trim_bytes, string.size() - last_non_ws_end_bytes};
}

absl::Cord TrimImpl(const absl::Cord& cord) {
  size_t left_trim_bytes = 0;
  {
    absl::Cord::CharIterator begin = cord.char_begin();
    const absl::Cord::CharIterator end = cord.char_end();
    while (begin != end) {
      char32_t c;
      size_t char_len;
      std::tie(c, char_len) = cel::internal::Utf8Decode(begin);
      if (!IsUnicodeWhitespace(c)) {
        break;
      }
      absl::Cord::Advance(&begin, char_len);
      left_trim_bytes += char_len;
    }
  }

  if (left_trim_bytes == cord.size()) {
    return absl::Cord();
  }

  absl::Cord ltrimmed =
      cord.Subcord(left_trim_bytes, cord.size() - left_trim_bytes);

  size_t last_non_ws_end_bytes = 0;
  size_t current_pos_bytes = 0;
  {
    absl::Cord::CharIterator begin = ltrimmed.char_begin();
    const absl::Cord::CharIterator end = ltrimmed.char_end();
    while (begin != end) {
      char32_t c;
      size_t char_len;
      std::tie(c, char_len) = cel::internal::Utf8Decode(begin);
      if (!IsUnicodeWhitespace(c)) {
        last_non_ws_end_bytes = current_pos_bytes + char_len;
      }
      absl::Cord::Advance(&begin, char_len);
      current_pos_bytes += char_len;
    }
  }
  return ltrimmed.Subcord(0, last_non_ws_end_bytes);
}

}  // namespace

StringValue StringValue::Trim() const {
  switch (value_.GetKind()) {
    case common_internal::ByteStringKind::kSmall: {
      std::pair<size_t, size_t> trims = (TrimImpl)(value_.GetSmall());
      StringValue result;
      result.value_.rep_.header.kind = common_internal::ByteStringKind::kSmall;
      result.value_.rep_.small.size =
          value_.rep_.small.size - trims.first - trims.second;
      std::memcpy(result.value_.rep_.small.data,
                  value_.rep_.small.data + trims.first,
                  result.value_.rep_.small.size);
      result.value_.rep_.small.arena = value_.GetSmallArena();
      return result;
    }
    case common_internal::ByteStringKind::kMedium: {
      std::pair<size_t, size_t> trims = (TrimImpl)(value_.GetMedium());
      StringValue result;
      result.value_.rep_.header.kind = common_internal::ByteStringKind::kMedium;
      result.value_.rep_.medium.size =
          value_.rep_.medium.size - trims.first - trims.second;
      result.value_.rep_.medium.data = value_.rep_.medium.data + trims.first;
      result.value_.rep_.medium.owner = value_.rep_.medium.owner;
      common_internal::StrongRef(result.value_.GetMediumReferenceCount());
      return result;
    }
    case common_internal::ByteStringKind::kLarge: {
      return StringValue::Wrap((TrimImpl)(value_.GetLarge()));
    }
  }
}

namespace {

void AppendQuoteCodePoint(char32_t code_point, std::string& dst) {
  switch (code_point) {
    case '\a':
      dst.append("\\a");
      break;
    case '\b':
      dst.append("\\b");
      break;
    case '\f':
      dst.append("\\f");
      break;
    case '\n':
      dst.append("\\n");
      break;
    case '\r':
      dst.append("\\r");
      break;
    case '\t':
      dst.append("\\t");
      break;
    case '\v':
      dst.append("\\v");
      break;
    case '\\':
      dst.append("\\\\");
      break;
    case '\"':
      dst.append("\\\"");
      break;
    default:
      cel::internal::Utf8Encode(code_point, &dst);
      break;
  }
}

}  // namespace

StringValue StringValue::Quote(google::protobuf::Arena* absl_nonnull arena) const {
  return value_.Visit(absl::Overload(
      [&](absl::string_view rep) -> StringValue {
        std::string result;
        result.push_back('\"');
        while (!rep.empty()) {
          char32_t code_point;
          size_t code_units;
          std::tie(code_point, code_units) = cel::internal::Utf8Decode(rep);
          AppendQuoteCodePoint(code_point, result);
          rep.remove_prefix(code_units);
        }
        result.push_back('\"');
        return StringValue::From(std::move(result), arena);
      },
      [&](const absl::Cord& rep) -> StringValue {
        absl::Cord::CharIterator begin = rep.char_begin();
        absl::Cord::CharIterator end = rep.char_end();
        std::string result;
        result.push_back('\"');
        while (begin != end) {
          char32_t code_point;
          size_t code_units;
          std::tie(code_point, code_units) = cel::internal::Utf8Decode(begin);
          AppendQuoteCodePoint(code_point, result);
          absl::Cord::Advance(&begin, code_units);
        }
        result.push_back('\"');
        return StringValue::From(std::move(result), arena);
      }));
}

StringValue StringValue::Reverse(google::protobuf::Arena* absl_nonnull arena) const {
  return value_.Visit(absl::Overload(
      [arena](absl::string_view string) -> StringValue {
        if (string.empty()) {
          return StringValue();
        }
        std::string reversed;
        reversed.reserve(string.size());
        const char* ptr = string.data() + string.size();
        const char* begin = string.data();
        while (ptr > begin) {
          const char* char_end = ptr;
          --ptr;
          // Back up to beginning of encoded UTF-8 code point.
          while (ptr > begin && (*ptr & 0xC0) == 0x80) {
            --ptr;
          }
          reversed.append(ptr, char_end - ptr);
        }
        return StringValue::From(std::move(reversed), arena);
      },
      [arena](const absl::Cord& cord) -> StringValue {
        if (cord.empty()) {
          return StringValue();
        }
        std::vector<char32_t> code_points;
        absl::Cord::CharIterator char_begin = cord.char_begin();
        absl::Cord::CharIterator char_end = cord.char_end();
        while (char_begin != char_end) {
          char32_t code_point;
          size_t code_units =
              cel::internal::Utf8Decode(char_begin, &code_point);
          code_points.push_back(code_point);
          absl::Cord::Advance(&char_begin, code_units);
        }
        std::string reversed;
        reversed.reserve(cord.size());
        for (auto it = code_points.rbegin(); it != code_points.rend(); ++it) {
          cel::internal::Utf8Encode(*it, &reversed);
        }
        return StringValue::From(std::move(reversed), arena);
      }));
}

absl::StatusOr<Value> StringValue::Join(
    const ListValue& list,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      Join(list, descriptor_pool, message_factory, arena, &result));
  return result;
}

absl::Status StringValue::Join(
    const ListValue& list,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  std::string joined;

  CEL_ASSIGN_OR_RETURN(auto iterator, list.NewIterator());

  CEL_ASSIGN_OR_RETURN(
      absl::optional<Value> element,
      iterator->Next1(descriptor_pool, message_factory, arena));
  if (element) {
    if (auto string_element = element->AsString(); string_element) {
      string_element->AppendToString(&joined);
    } else {
      ABSL_DCHECK(!element->Is<ErrorValue>());
      *result =
          ErrorValue(runtime_internal::CreateNoMatchingOverloadError("join"));
      return absl::OkStatus();
    }
    while (true) {
      CEL_ASSIGN_OR_RETURN(
          element, iterator->Next1(descriptor_pool, message_factory, arena));
      if (!element) {
        break;
      }
      AppendToString(&joined);
      if (auto string_element = element->AsString(); string_element) {
        string_element->AppendToString(&joined);
      } else {
        ABSL_DCHECK(!element->Is<ErrorValue>());
        *result =
            ErrorValue(runtime_internal::CreateNoMatchingOverloadError("join"));
        return absl::OkStatus();
      }
    }
  }

  if (joined.size() > common_internal::kSmallByteStringCapacity) {
    joined.shrink_to_fit();
  }

  *result = StringValue::From(std::move(joined), arena);
  return absl::OkStatus();
}

absl::StatusOr<Value> StringValue::Split(
    const StringValue& delimiter, int64_t limit,
    google::protobuf::Arena* absl_nonnull arena) const {
  Value result;
  CEL_RETURN_IF_ERROR(Split(delimiter, limit, arena, &result));
  return result;
}

absl::Status StringValue::Split(const StringValue& delimiter,
                                google::protobuf::Arena* absl_nonnull arena,
                                Value* absl_nonnull result) const {
  return Split(delimiter, -1, arena, result);
}

absl::StatusOr<Value> StringValue::Split(
    const StringValue& delimiter, google::protobuf::Arena* absl_nonnull arena) const {
  Value result;
  CEL_RETURN_IF_ERROR(Split(delimiter, -1, arena, &result));
  return result;
}

absl::Status StringValue::Split(const StringValue& delimiter, int64_t limit,
                                google::protobuf::Arena* absl_nonnull arena,
                                Value* absl_nonnull result) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (limit == 0) {
    // Per spec, when limit is 0 return an empty list.
    *result = ListValue();
    return absl::OkStatus();
  }
  if (limit < 0) {
    // Per spec, when limit is negative treat it as unlimited splits.
    limit = std::numeric_limits<int64_t>::max();
  }

  std::vector<std::pair<size_t, size_t>> splits;
  size_t pos = 0;
  const size_t len = value_.size();

  if (delimiter.IsEmpty()) {
    value_.Visit(absl::Overload(
        [&](absl::string_view s) {
          while (pos < len && limit > 1) {
            size_t char_len = cel::internal::Utf8Decode(s.substr(pos), nullptr);
            splits.push_back({pos, pos + char_len});
            pos += char_len;
            --limit;
          }
        },
        [&](const absl::Cord& s) {
          while (pos < len && limit > 1) {
            size_t char_len = cel::internal::Utf8Decode(
                s.Subcord(pos, len - pos).char_begin(), nullptr);
            splits.push_back({pos, pos + char_len});
            pos += char_len;
            --limit;
          }
        }));
  } else {
    while (pos < len && limit > 1) {
      absl::optional<size_t> next = value_.Find(delimiter.value_, pos);
      if (!next) {
        break;
      }
      splits.push_back(std::pair{pos, *next});
      pos = *next + delimiter.value_.size();
      --limit;
      ABSL_DCHECK_LE(pos, len);
    }
  }

  if (splits.empty() || !delimiter.IsEmpty() || pos < len) {
    splits.push_back(std::pair{pos, len});
  }

  auto builder = NewListValueBuilder(arena);
  builder->Reserve(splits.size());
  for (const std::pair<size_t, size_t>& split : splits) {
    builder->UnsafeAdd(
        StringValue(value_.Substring(split.first, split.second)));
  }
  *result = std::move(*builder).Build();
  return absl::OkStatus();
}

absl::StatusOr<Value> StringValue::Replace(
    const StringValue& needle, const StringValue& replacement, int64_t limit,
    google::protobuf::Arena* absl_nonnull arena) const {
  Value result;
  CEL_RETURN_IF_ERROR(Replace(needle, replacement, limit, arena, &result));
  return result;
}

absl::Status StringValue::Replace(const StringValue& needle,
                                  const StringValue& replacement,
                                  google::protobuf::Arena* absl_nonnull arena,
                                  Value* absl_nonnull result) const {
  return Replace(needle, replacement, -1, arena, result);
}

absl::StatusOr<Value> StringValue::Replace(
    const StringValue& needle, const StringValue& replacement,
    google::protobuf::Arena* absl_nonnull arena) const {
  Value result;
  CEL_RETURN_IF_ERROR(Replace(needle, replacement, -1, arena, &result));
  return result;
}

absl::Status StringValue::Replace(const StringValue& needle,
                                  const StringValue& replacement, int64_t limit,
                                  google::protobuf::Arena* absl_nonnull arena,
                                  Value* absl_nonnull result) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (limit == 0) {
    // Per spec, when limit is 0 return the original string.
    *result = *this;
    return absl::OkStatus();
  }
  if (limit < 0) {
    // Per spec, when limit is negative treat it as unlimited replacements.
    limit = std::numeric_limits<int64_t>::max();
  }

  size_t pos = 0;
  const size_t len = value_.size();
  const size_t needle_len = needle.value_.size();
  std::string res_str;

  if (needle.IsEmpty()) {
    value_.Visit(absl::Overload(
        [&](absl::string_view s) {
          while (pos < len && limit > 0) {
            replacement.AppendToString(&res_str);
            size_t char_len = cel::internal::Utf8Decode(s.substr(pos), nullptr);
            value_.Substring(pos, pos + char_len).AppendToString(&res_str);
            pos += char_len;
            --limit;
          }
        },
        [&](const absl::Cord& s) {
          while (pos < len && limit > 0) {
            replacement.AppendToString(&res_str);
            size_t char_len = cel::internal::Utf8Decode(
                s.Subcord(pos, len - pos).char_begin(), nullptr);
            value_.Substring(pos, pos + char_len).AppendToString(&res_str);
            pos += char_len;
            --limit;
          }
        }));
    if (limit > 0) {
      replacement.AppendToString(&res_str);
    }
  } else {
    while (pos < len && limit > 0) {
      absl::optional<size_t> next = value_.Find(needle.value_, pos);
      if (!next) {
        break;
      }

      value_.Substring(pos, *next).AppendToString(&res_str);
      replacement.AppendToString(&res_str);

      pos = *next + needle_len;
      --limit;
    }
  }

  if (pos < len) {
    value_.Substring(pos, len).AppendToString(&res_str);
  }

  if (res_str.size() > common_internal::kSmallByteStringCapacity) {
    res_str.shrink_to_fit();
  }

  *result = StringValue::From(std::move(res_str), arena);
  return absl::OkStatus();
}

Value StringValue::CharAt(int64_t pos) const {
  if (pos < 0) {
    return ErrorValue(absl::InvalidArgumentError(
        "<string>.charAt(<pos>): <pos> is less than 0"));
  }
  return value_.Visit(absl::Overload(
      [this, pos](absl::string_view rep) mutable -> Value {
        while (!rep.empty()) {
          char32_t code_point;
          size_t code_units;
          std::tie(code_point, code_units) = cel::internal::Utf8Decode(rep);
          if (pos == 0) {
            StringValue result;
            result.value_.rep_.header.kind =
                common_internal::ByteStringKind::kSmall;
            result.value_.rep_.small.size = cel::internal::Utf8Encode(
                code_point, result.value_.rep_.small.data);
            result.value_.rep_.small.arena = value_.GetArena();
            return result;
          }
          rep.remove_prefix(code_units);
          --pos;
        }
        // If we exit the loop, we iterated through all the code points in
        // `rep`. `pos == 0` means we were looking for a character at index
        // `size()`, which is defined to return an empty string.
        if (pos == 0) {
          return StringValue();
        }
        return ErrorValue(absl::InvalidArgumentError(
            "<string>.charAt(<pos>): <pos> is greater than <string>.size()"));
      },
      [pos](const absl::Cord& rep) mutable -> Value {
        absl::Cord::CharIterator begin = rep.char_begin();
        absl::Cord::CharIterator end = rep.char_end();
        while (begin != end) {
          char32_t code_point;
          size_t code_units;
          std::tie(code_point, code_units) = cel::internal::Utf8Decode(begin);
          if (pos == 0) {
            StringValue result;
            result.value_.rep_.header.kind =
                common_internal::ByteStringKind::kSmall;
            result.value_.rep_.small.size = cel::internal::Utf8Encode(
                code_point, result.value_.rep_.small.data);
            result.value_.rep_.small.arena = nullptr;
            return result;
          }
          absl::Cord::Advance(&begin, code_units);
          --pos;
        }
        // If we exit the loop, we iterated through all the code points in
        // `rep`. `pos == 0` means we were looking for a character at index
        // `size()`, which is defined to return an empty string.
        if (pos == 0) {
          return StringValue();
        }
        return ErrorValue(absl::InvalidArgumentError(
            "<string>.charAt(<pos>): <pos> is greater than <string>.size()"));
      }));
}

}  // namespace cel
