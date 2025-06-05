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

// Utilities for decoding and encoding the protocol buffer wire format. CEL
// requires supporting `google.protobuf.Any`. The core of CEL cannot take a
// direct dependency on protobuf and utilities for encoding/decoding varint and
// fixed64 are not part of Abseil. So we either would have to either reject
// `google.protobuf.Any` when protobuf is not linked or implement the utilities
// ourselves. We chose the latter as it is the lesser of two evils and
// introduces significantly less complexity compared to the former.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_WIRE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_WIRE_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/numeric/bits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_buffer.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace cel::internal {

// Calculates the number of bytes required to encode the unsigned integral `x`
// using varint.
template <typename T>
inline constexpr std::enable_if_t<
    (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 8), size_t>
VarintSize(T x) {
  return static_cast<size_t>(
      (static_cast<uint32_t>((sizeof(T) * 8 - 1) -
                             absl::countl_zero<T>(x | T{1})) *
           9 +
       73) /
      64);
}

// Overload of `VarintSize()` handling signed 64-bit integrals.
inline constexpr size_t VarintSize(int64_t x) {
  return VarintSize(static_cast<uint64_t>(x));
}

// Overload of `VarintSize()` handling signed 32-bit integrals.
inline constexpr size_t VarintSize(int32_t x) {
  // Sign-extend to 64-bits, then size.
  return VarintSize(static_cast<int64_t>(x));
}

// Overload of `VarintSize()` for bool.
inline constexpr size_t VarintSize(bool x ABSL_ATTRIBUTE_UNUSED) { return 1; }

// Compile-time constant for the size required to encode any value of the
// integral type `T` using varint.
template <typename T>
inline constexpr size_t kMaxVarintSize = VarintSize(static_cast<T>(~T{0}));

// Instantiation of `kMaxVarintSize` for bool to prevent bitwise negation of a
// bool warning.
template <>
inline constexpr size_t kMaxVarintSize<bool> = 1;

// Enumeration of the protocol buffer wire tags, see
// https://protobuf.dev/programming-guides/encoding/#structure.
enum class ProtoWireType : uint32_t {
  kVarint = 0,
  kFixed64 = 1,
  kLengthDelimited = 2,
  kStartGroup = 3,
  kEndGroup = 4,
  kFixed32 = 5,
};

inline constexpr uint32_t kProtoWireTypeMask = uint32_t{0x7};
inline constexpr int kFieldNumberShift = 3;

class ProtoWireTag final {
 public:
  static constexpr uint32_t kTypeMask = uint32_t{0x7};
  static constexpr int kFieldNumberShift = 3;

  constexpr explicit ProtoWireTag(uint32_t tag) : tag_(tag) {}

  constexpr ProtoWireTag(uint32_t field_number, ProtoWireType type)
      : ProtoWireTag((field_number << kFieldNumberShift) |
                     static_cast<uint32_t>(type)) {
    ABSL_ASSERT(((field_number << kFieldNumberShift) >> kFieldNumberShift) ==
                field_number);
  }

  constexpr uint32_t field_number() const { return tag_ >> kFieldNumberShift; }

  constexpr ProtoWireType type() const {
    return static_cast<ProtoWireType>(tag_ & kTypeMask);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator uint32_t() const { return tag_; }

 private:
  uint32_t tag_;
};

inline constexpr bool ProtoWireTypeIsValid(ProtoWireType type) {
  // Ensure `type` is only [0-5]. The bitmask for `type` is 0x7 which allows 6
  // to exist, but that is not used and invalid. We detect that here.
  return (static_cast<uint32_t>(type) & uint32_t{0x7}) ==
             static_cast<uint32_t>(type) &&
         static_cast<uint32_t>(type) != uint32_t{0x6};
}

// Creates the "tag" of a record, see
// https://protobuf.dev/programming-guides/encoding/#structure.
inline constexpr uint32_t MakeProtoWireTag(uint32_t field_number,
                                           ProtoWireType type) {
  ABSL_ASSERT(((field_number << 3) >> 3) == field_number);
  return (field_number << 3) | static_cast<uint32_t>(type);
}

// Encodes `value` as varint and stores it in `buffer`. This method should not
// be used outside of this header.
inline size_t VarintEncodeUnsafe(uint64_t value, char* buffer) {
  size_t length = 0;
  while (ABSL_PREDICT_FALSE(value >= 0x80)) {
    buffer[length++] = static_cast<char>(static_cast<uint8_t>(value | 0x80));
    value >>= 7;
  }
  buffer[length++] = static_cast<char>(static_cast<uint8_t>(value));
  return length;
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(uint64_t value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch[kMaxVarintSize<uint64_t>];
  buffer.Append(absl::string_view(scratch, VarintEncodeUnsafe(value, scratch)));
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(int64_t value, absl::Cord& buffer) {
  return VarintEncode(absl::bit_cast<uint64_t>(value), buffer);
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(uint32_t value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch[kMaxVarintSize<uint32_t>];
  buffer.Append(absl::string_view(scratch, VarintEncodeUnsafe(value, scratch)));
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(int32_t value, absl::Cord& buffer) {
  // Sign-extend to 64-bits, then encode.
  return VarintEncode(static_cast<int64_t>(value), buffer);
}

// Encodes `value` as varint and appends it to `buffer`.
inline void VarintEncode(bool value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch = value ? char{1} : char{0};
  buffer.Append(absl::string_view(&scratch, 1));
}

inline void Fixed32EncodeUnsafe(uint64_t value, char* buffer) {
  buffer[0] = static_cast<char>(static_cast<uint8_t>(value));
  buffer[1] = static_cast<char>(static_cast<uint8_t>(value >> 8));
  buffer[2] = static_cast<char>(static_cast<uint8_t>(value >> 16));
  buffer[3] = static_cast<char>(static_cast<uint8_t>(value >> 24));
}

// Encodes `value` as a fixed-size number, see
// https://protobuf.dev/programming-guides/encoding/#non-varint-numbers.
inline void Fixed32Encode(uint32_t value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch[4];
  Fixed32EncodeUnsafe(value, scratch);
  buffer.Append(absl::string_view(scratch, ABSL_ARRAYSIZE(scratch)));
}

// Encodes `value` as a fixed-size number, see
// https://protobuf.dev/programming-guides/encoding/#non-varint-numbers.
inline void Fixed32Encode(float value, absl::Cord& buffer) {
  Fixed32Encode(absl::bit_cast<uint32_t>(value), buffer);
}

inline void Fixed64EncodeUnsafe(uint64_t value, char* buffer) {
  buffer[0] = static_cast<char>(static_cast<uint8_t>(value));
  buffer[1] = static_cast<char>(static_cast<uint8_t>(value >> 8));
  buffer[2] = static_cast<char>(static_cast<uint8_t>(value >> 16));
  buffer[3] = static_cast<char>(static_cast<uint8_t>(value >> 24));
  buffer[4] = static_cast<char>(static_cast<uint8_t>(value >> 32));
  buffer[5] = static_cast<char>(static_cast<uint8_t>(value >> 40));
  buffer[6] = static_cast<char>(static_cast<uint8_t>(value >> 48));
  buffer[7] = static_cast<char>(static_cast<uint8_t>(value >> 56));
}

// Encodes `value` as a fixed-size number, see
// https://protobuf.dev/programming-guides/encoding/#non-varint-numbers.
inline void Fixed64Encode(uint64_t value, absl::Cord& buffer) {
  // `absl::Cord::GetAppendBuffer` will allocate a block regardless of whether
  // `buffer` has enough inline storage space left. To take advantage of inline
  // storage space, we need to just do a plain append.
  char scratch[8];
  Fixed64EncodeUnsafe(value, scratch);
  buffer.Append(absl::string_view(scratch, ABSL_ARRAYSIZE(scratch)));
}

// Encodes `value` as a fixed-size number, see
// https://protobuf.dev/programming-guides/encoding/#non-varint-numbers.
inline void Fixed64Encode(double value, absl::Cord& buffer) {
  Fixed64Encode(absl::bit_cast<uint64_t>(value), buffer);
}

template <typename T>
struct VarintDecodeResult {
  T value;
  size_t size_bytes;
};

// Decodes an unsigned integral from `data` which was previously encoded as a
// varint.
template <typename T>
inline std::enable_if_t<std::is_integral<T>::value &&
                            std::is_unsigned<T>::value,
                        absl::optional<VarintDecodeResult<T>>>
VarintDecode(const absl::Cord& data) {
  uint64_t result = 0;
  int count = 0;
  uint64_t b;
  auto begin = data.char_begin();
  auto end = data.char_end();
  do {
    if (ABSL_PREDICT_FALSE(count == kMaxVarintSize<T>)) {
      return absl::nullopt;
    }
    if (ABSL_PREDICT_FALSE(begin == end)) {
      return absl::nullopt;
    }
    b = static_cast<uint8_t>(*begin);
    result |= (b & uint64_t{0x7f}) << (7 * count);
    ++begin;
    ++count;
  } while (ABSL_PREDICT_FALSE(b & uint64_t{0x80}));
  if (ABSL_PREDICT_FALSE(result > std::numeric_limits<T>::max())) {
    return absl::nullopt;
  }
  return VarintDecodeResult<T>{static_cast<T>(result),
                               static_cast<size_t>(count)};
}

// Decodes an signed integral from `data` which was previously encoded as a
// varint.
template <typename T>
inline std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value,
                        absl::optional<VarintDecodeResult<T>>>
VarintDecode(const absl::Cord& data) {
  // We have to read the full maximum varint, as negative values are encoded as
  // 10 bytes.
  if (auto value = VarintDecode<uint64_t>(data);
      ABSL_PREDICT_TRUE(value.has_value())) {
    if (ABSL_PREDICT_TRUE(absl::bit_cast<int64_t>(value->value) >=
                              std::numeric_limits<T>::min() &&
                          absl::bit_cast<int64_t>(value->value) <=
                              std::numeric_limits<T>::max())) {
      return VarintDecodeResult<T>{
          static_cast<T>(absl::bit_cast<int64_t>(value->value)),
          value->size_bytes};
    }
  }
  return absl::nullopt;
}

template <typename T>
inline std::enable_if_t<((std::is_integral<T>::value &&
                          std::is_unsigned<T>::value) ||
                         std::is_floating_point<T>::value) &&
                            sizeof(T) == 8,
                        absl::optional<T>>
Fixed64Decode(const absl::Cord& data) {
  if (ABSL_PREDICT_FALSE(data.size() < 8)) {
    return absl::nullopt;
  }
  uint64_t result = 0;
  auto it = data.char_begin();
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it));
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 8;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 16;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 24;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 32;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 40;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 48;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 56;
  return absl::bit_cast<T>(result);
}

template <typename T>
inline std::enable_if_t<((std::is_integral<T>::value &&
                          std::is_unsigned<T>::value) ||
                         std::is_floating_point<T>::value) &&
                            sizeof(T) == 4,
                        absl::optional<T>>
Fixed32Decode(const absl::Cord& data) {
  if (ABSL_PREDICT_FALSE(data.size() < 4)) {
    return absl::nullopt;
  }
  uint32_t result = 0;
  auto it = data.char_begin();
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it));
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 8;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 16;
  ++it;
  result |= static_cast<uint64_t>(static_cast<uint8_t>(*it)) << 24;
  return absl::bit_cast<T>(result);
}

inline absl::optional<ProtoWireTag> DecodeProtoWireTag(uint32_t value) {
  if (ABSL_PREDICT_FALSE((value >> ProtoWireTag::kFieldNumberShift) == 0)) {
    // Field number is 0.
    return absl::nullopt;
  }
  if (ABSL_PREDICT_FALSE(!ProtoWireTypeIsValid(
          static_cast<ProtoWireType>(value & ProtoWireTag::kTypeMask)))) {
    // Wire type is 6, only 0-5 are used.
    return absl::nullopt;
  }
  return ProtoWireTag(value);
}

inline absl::optional<ProtoWireTag> DecodeProtoWireTag(uint64_t value) {
  if (ABSL_PREDICT_FALSE(value > std::numeric_limits<uint32_t>::max())) {
    // Tags are only supposed to be 32-bit varints.
    return absl::nullopt;
  }
  return DecodeProtoWireTag(static_cast<uint32_t>(value));
}

// Skips the next length and/or value in `data` which has a wire type `type`.
// `data` must point to the byte immediately after the tag which encoded `type`.
// Returns `true` on success, `false` otherwise.
ABSL_MUST_USE_RESULT bool SkipLengthValue(absl::Cord& data, ProtoWireType type);

class ProtoWireDecoder {
 public:
  ProtoWireDecoder(absl::string_view message ABSL_ATTRIBUTE_LIFETIME_BOUND,
                   const absl::Cord& data)
      : message_(message), data_(data) {}

  bool HasNext() const {
    ABSL_DCHECK(!tag_.has_value());
    return !data_.empty();
  }

  absl::StatusOr<ProtoWireTag> ReadTag();

  absl::Status SkipLengthValue();

  template <typename T>
  std::enable_if_t<std::is_integral<T>::value, absl::StatusOr<T>> ReadVarint() {
    ABSL_DCHECK(tag_.has_value() && tag_->type() == ProtoWireType::kVarint);
    auto result = internal::VarintDecode<T>(data_);
    if (ABSL_PREDICT_FALSE(!result.has_value())) {
      return absl::DataLossError(absl::StrCat(
          "malformed or out of range varint encountered decoding field ",
          tag_->field_number(), " of ", message_));
    }
    data_.RemovePrefix(result->size_bytes);
    tag_.reset();
    return result->value;
  }

  template <typename T>
  std::enable_if_t<((std::is_integral<T>::value &&
                     std::is_unsigned<T>::value) ||
                    std::is_floating_point<T>::value) &&
                       sizeof(T) == 4,
                   absl::StatusOr<T>>
  ReadFixed32() {
    ABSL_DCHECK(tag_.has_value() && tag_->type() == ProtoWireType::kFixed32);
    auto result = internal::Fixed32Decode<T>(data_);
    if (ABSL_PREDICT_FALSE(!result.has_value())) {
      return absl::DataLossError(
          absl::StrCat("malformed fixed32 encountered decoding field ",
                       tag_->field_number(), " of ", message_));
    }
    data_.RemovePrefix(4);
    tag_.reset();
    return *result;
  }

  template <typename T>
  std::enable_if_t<((std::is_integral<T>::value &&
                     std::is_unsigned<T>::value) ||
                    std::is_floating_point<T>::value) &&
                       sizeof(T) == 8,
                   absl::StatusOr<T>>
  ReadFixed64() {
    ABSL_DCHECK(tag_.has_value() && tag_->type() == ProtoWireType::kFixed64);
    auto result = internal::Fixed64Decode<T>(data_);
    if (ABSL_PREDICT_FALSE(!result.has_value())) {
      return absl::DataLossError(
          absl::StrCat("malformed fixed64 encountered decoding field ",
                       tag_->field_number(), " of ", message_));
    }
    data_.RemovePrefix(8);
    tag_.reset();
    return *result;
  }

  absl::StatusOr<absl::Cord> ReadLengthDelimited();

  void EnsureFullyDecoded() { ABSL_DCHECK(data_.empty()); }

 private:
  absl::string_view message_;
  absl::Cord data_;
  absl::optional<ProtoWireTag> tag_;
};

class ProtoWireEncoder final {
 public:
  explicit ProtoWireEncoder(absl::string_view message
                                ABSL_ATTRIBUTE_LIFETIME_BOUND,
                            absl::Cord& data ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : message_(message), data_(data), original_data_size_(data_.size()) {}

  bool empty() const { return size() == 0; }

  size_t size() const { return data_.size() - original_data_size_; }

  absl::Status WriteTag(ProtoWireTag tag);

  template <typename T>
  std::enable_if_t<std::is_integral_v<T>, absl::Status> WriteVarint(T value) {
    ABSL_DCHECK(tag_.has_value() && tag_->type() == ProtoWireType::kVarint);
    VarintEncode(value, data_);
    tag_.reset();
    return absl::OkStatus();
  }

  template <typename T>
  std::enable_if_t<sizeof(T) == 4 &&
                       (std::is_integral_v<T> || std::is_floating_point_v<T>),
                   absl::Status>
  WriteFixed32(T value) {
    ABSL_DCHECK(tag_.has_value() && tag_->type() == ProtoWireType::kFixed32);
    Fixed32Encode(value, data_);
    tag_.reset();
    return absl::OkStatus();
  }

  template <typename T>
  std::enable_if_t<sizeof(T) == 8 &&
                       (std::is_integral_v<T> || std::is_floating_point_v<T>),
                   absl::Status>
  WriteFixed64(T value) {
    ABSL_DCHECK(tag_.has_value() && tag_->type() == ProtoWireType::kFixed64);
    Fixed64Encode(value, data_);
    tag_.reset();
    return absl::OkStatus();
  }

  absl::Status WriteLengthDelimited(absl::Cord data);

  absl::Status WriteLengthDelimited(absl::string_view data);

  void EnsureFullyEncoded() { ABSL_DCHECK(!tag_.has_value()); }

 private:
  absl::string_view message_;
  absl::Cord& data_;
  const size_t original_data_size_;
  absl::optional<ProtoWireTag> tag_;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_WIRE_H_
