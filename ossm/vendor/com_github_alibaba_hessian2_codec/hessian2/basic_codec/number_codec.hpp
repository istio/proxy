#pragma once

#include "hessian2/codec.hpp"

namespace Hessian2 {

namespace {
template <typename T>
typename std::enable_if<
    std::is_signed<T>::value && (sizeof(T) > sizeof(int8_t)), T>::type
LeftShift(int8_t left, uint16_t bit_number) {
  if (left < 0) {
    left = left * -1;
    return -1 * (left << bit_number);
  }
  return left << bit_number;
}

template <typename T, size_t Size>
typename std::enable_if<std::is_floating_point<T>::value, T>::type readBE(
    ReaderPtr &reader) {
  static_assert(sizeof(T) == 8, "Only support double type");
  static_assert(Size == 8 || Size == 4, "Only support 4 or 8 size");
  double out;
  if (Size == 8) {
    auto in = reader->readBE<uint64_t>();
    ABSL_ASSERT(in.first);
    std::memcpy(&out, &in.second, 8);
    return out;
  }

  // Frankly, I don't know why I'm doing this, I'm just referring to the Java
  // implementation.
  ABSL_ASSERT(Size == 4);
  auto in = reader->readBE<int32_t>();
  ABSL_ASSERT(in.first);
  return 0.001 * in.second;
}

}  // namespace

/////////////////////////////////////////
// Double
/////////////////////////////////////////

template <>
std::unique_ptr<double> Decoder::decode();

template <>
bool Encoder::encode(const double &value);

/////////////////////////////////////////
// Int32
/////////////////////////////////////////

template <>
std::unique_ptr<int32_t> Decoder::decode();

template <>
bool Encoder::encode(const int32_t &data);

/////////////////////////////////////////
// Int64
/////////////////////////////////////////

template <>
std::unique_ptr<int64_t> Decoder::decode();

template <>
bool Encoder::encode(const int64_t &data);

template <>
bool Encoder::encode(const int8_t &data);

template <>
bool Encoder::encode(const int16_t &data);

template <>
bool Encoder::encode(const uint8_t &data);

template <>
bool Encoder::encode(const uint16_t &data);

template <>
bool Encoder::encode(const uint32_t &data);

// Encoding and decoding of uint64_t is not supported because Java 64-bit
// integers are signed.

}  // namespace Hessian2
