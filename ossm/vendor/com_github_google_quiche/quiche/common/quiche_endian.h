// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_COMMON_QUICHE_ENDIAN_H_
#define QUICHE_COMMON_QUICHE_ENDIAN_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>

#include "quiche/common/platform/api/quiche_export.h"

namespace quiche {

enum Endianness {
  NETWORK_BYTE_ORDER,  // big endian
  HOST_BYTE_ORDER      // little endian
};

enum HostEndianness {
  BIG,
  LITTLE
};

// Provide utility functions that convert from/to network order (big endian)
// to/from host order (little endian).
class QUICHE_EXPORT QuicheEndian {
 public:
  // Get host machine endianness
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  static const quiche::HostEndianness HostEndianness = quiche::BIG;
#else
  static const quiche::HostEndianness HostEndianness = quiche::LITTLE;
#endif

  // Convert byte order of |x|.
 #if defined(__clang__) || \
    (defined(__GNUC__) && \
     ((__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || __GNUC__ >= 5))
  static uint16_t ByteSwap16(uint16_t x) { return __builtin_bswap16(x); }
  static uint32_t ByteSwap32(uint32_t x) { return __builtin_bswap32(x); }
  static uint64_t ByteSwap64(uint64_t x) { return __builtin_bswap64(x); }
#else
  static uint16_t ByteSwap16(uint16_t x) { return PortableByteSwap(x); }
  static uint32_t ByteSwap32(uint32_t x) { return PortableByteSwap(x); }
  static uint64_t ByteSwap64(uint64_t x) { return PortableByteSwap(x); }
#endif

  // Convert |x| from host order to network order (big endian).
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  static uint16_t HostToNet16(uint16_t x) { return x; }
  static uint32_t HostToNet32(uint32_t x) { return x; }
  static uint64_t HostToNet64(uint64_t x) { return x; }
#else
  static uint16_t HostToNet16(uint16_t x) { return ByteSwap16(x); }
  static uint32_t HostToNet32(uint32_t x) { return ByteSwap32(x); }
  static uint64_t HostToNet64(uint64_t x) { return ByteSwap64(x); }
#endif

  // Convert |x| from network order (big endian) to host order.
  static uint16_t NetToHost16(uint16_t x) { return HostToNet16(x); }
  static uint32_t NetToHost32(uint32_t x) { return HostToNet32(x); }
  static uint64_t NetToHost64(uint64_t x) { return HostToNet64(x); }

  // Convert |x| from host order to little endian order.
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  static uint16_t HostToLittleEndian16(uint16_t x) { return ByteSwap16(x); }
  static uint32_t HostToLittleEndian32(uint32_t x) { return ByteSwap32(x); }
  static uint64_t HostToLittleEndian64(uint64_t x) { return ByteSwap64(x); }
#else
  static uint16_t HostToLittleEndian16(uint16_t x) { return x; }
  static uint32_t HostToLittleEndian32(uint32_t x) { return x; }
  static uint64_t HostToLittleEndian64(uint64_t x) { return x; }
#endif

  // Left public for tests.
  template <typename T>
  static T PortableByteSwap(T input) {
    static_assert(std::is_unsigned<T>::value, "T has to be uintNN_t");
    union {
      T number;
      char bytes[sizeof(T)];
    } value;
    value.number = input;
    std::reverse(&value.bytes[0], &value.bytes[sizeof(T)]);
    return value.number;
  }
};

enum QuicheVariableLengthIntegerLength : uint8_t {
  // Length zero means the variable length integer is not present.
  VARIABLE_LENGTH_INTEGER_LENGTH_0 = 0,
  VARIABLE_LENGTH_INTEGER_LENGTH_1 = 1,
  VARIABLE_LENGTH_INTEGER_LENGTH_2 = 2,
  VARIABLE_LENGTH_INTEGER_LENGTH_4 = 4,
  VARIABLE_LENGTH_INTEGER_LENGTH_8 = 8,

  // By default we write the IETF long header length using the 2-byte encoding
  // of variable length integers, even when the length is below 64, which allows
  // us to fill in the length before knowing what the length actually is.
  kQuicheDefaultLongHeaderLengthLength = VARIABLE_LENGTH_INTEGER_LENGTH_2,
};

constexpr std::array<QuicheVariableLengthIntegerLength, 5>
    kAllQuicheVariableLengthIntegerLengths = {
        VARIABLE_LENGTH_INTEGER_LENGTH_0, VARIABLE_LENGTH_INTEGER_LENGTH_1,
        VARIABLE_LENGTH_INTEGER_LENGTH_2, VARIABLE_LENGTH_INTEGER_LENGTH_4,
        VARIABLE_LENGTH_INTEGER_LENGTH_8,
};

}  // namespace quiche

#endif  // QUICHE_COMMON_QUICHE_ENDIAN_H_
