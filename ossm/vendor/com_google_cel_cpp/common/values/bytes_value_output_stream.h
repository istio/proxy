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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_OUTPUT_STREAM_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_OUTPUT_STREAM_H_

#include <cstdint>
#include <new>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/internal/byte_string.h"
#include "common/values/bytes_value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {

class BytesValueOutputStream final : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  explicit BytesValueOutputStream(const BytesValue& value)
      : BytesValueOutputStream(value, /*arena=*/nullptr) {}

  BytesValueOutputStream(const BytesValue& value,
                         google::protobuf::Arena* absl_nullable arena) {
    Construct(value, arena);
  }

  bool Next(void** data, int* size) override {
    return absl::visit(absl::Overload(
                           [&data, &size](String& string) -> bool {
                             return string.stream.Next(data, size);
                           },
                           [&data, &size](Cord& cord) -> bool {
                             return cord.Next(data, size);
                           }),
                       AsVariant());
  }

  void BackUp(int count) override {
    absl::visit(
        absl::Overload(
            [&count](String& string) -> void { string.stream.BackUp(count); },
            [&count](Cord& cord) -> void { cord.BackUp(count); }),
        AsVariant());
  }

  int64_t ByteCount() const override {
    return absl::visit(
        absl::Overload(
            [](const String& string) -> int64_t {
              return string.stream.ByteCount();
            },
            [](const Cord& cord) -> int64_t { return cord.ByteCount(); }),
        AsVariant());
  }

  bool WriteAliasedRaw(const void* data, int size) override {
    return absl::visit(absl::Overload(
                           [&data, &size](String& string) -> bool {
                             return string.stream.WriteAliasedRaw(data, size);
                           },
                           [&data, &size](Cord& cord) -> bool {
                             return cord.WriteAliasedRaw(data, size);
                           }),
                       AsVariant());
  }

  bool AllowsAliasing() const override {
    return absl::visit(
        absl::Overload(
            [](const String& string) -> bool {
              return string.stream.AllowsAliasing();
            },
            [](const Cord& cord) -> bool { return cord.AllowsAliasing(); }),
        AsVariant());
  }

  bool WriteCord(const absl::Cord& out) override {
    return absl::visit(
        absl::Overload(
            [&out](String& string) -> bool {
              return string.stream.WriteCord(out);
            },
            [&out](Cord& cord) -> bool { return cord.WriteCord(out); }),
        AsVariant());
  }

  BytesValue Consume() && {
    return absl::visit(absl::Overload(
                           [](String& string) -> BytesValue {
                             return BytesValue(string.arena,
                                               std::move(string.target));
                           },
                           [](Cord& cord) -> BytesValue {
                             return BytesValue(cord.Consume());
                           }),
                       AsVariant());
  }

 private:
  struct String final {
    String(absl::string_view target, google::protobuf::Arena* absl_nullable arena)
        : target(target), stream(&this->target), arena(arena) {}

    std::string target;
    google::protobuf::io::StringOutputStream stream;
    google::protobuf::Arena* absl_nullable arena;
  };

  using Cord = google::protobuf::io::CordOutputStream;

  using Variant = absl::variant<String, Cord>;

  void Construct(const BytesValue& value, google::protobuf::Arena* absl_nullable arena) {
    switch (value.value_.GetKind()) {
      case common_internal::ByteStringKind::kSmall:
        Construct(value.value_.GetSmall(), arena);
        break;
      case common_internal::ByteStringKind::kMedium:
        Construct(value.value_.GetMedium(), arena);
        break;
      case common_internal::ByteStringKind::kLarge:
        Construct(value.value_.GetLarge());
        break;
    }
  }

  void Construct(absl::string_view value, google::protobuf::Arena* absl_nullable arena) {
    ::new (static_cast<void*>(&impl_[0]))
        Variant(absl::in_place_type<String>, value, arena);
  }

  void Construct(const absl::Cord& value) {
    ::new (static_cast<void*>(&impl_[0]))
        Variant(absl::in_place_type<Cord>, value);
  }

  void Destruct() { AsVariant().~variant(); }

  Variant& AsVariant() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *std::launder(reinterpret_cast<Variant*>(&impl_[0]));
  }

  const Variant& AsVariant() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *std::launder(reinterpret_cast<const Variant*>(&impl_[0]));
  }

  alignas(Variant) char impl_[sizeof(Variant)];
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_OUTPUT_STREAM_H_
