// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"

template <class NumberType, class StringPieceType, class StringType>
void CheckRoundtripsT(const uint8_t* data,
                      const size_t size,
                      StringType (*num_to_string)(NumberType),
                      bool (*string_to_num)(StringPieceType, NumberType*)) {
  // Ensure we can read a NumberType from |data|
  if (size < sizeof(NumberType))
    return;
  const NumberType v1 = *reinterpret_cast<const NumberType*>(data);

  // Because we started with an arbitrary NumberType value, not an arbitrary
  // string, we expect that the function |string_to_num| (e.g. StringToInt) will
  // return true, indicating a perfect conversion.
  NumberType v2;
  GURL_CHECK(string_to_num(num_to_string(v1), &v2));

  // Given that this was a perfect conversion, we expect the original NumberType
  // value to equal the newly parsed one.
  GURL_CHECK_EQ(v1, v2);
}

template <class NumberType>
void CheckRoundtrips(const uint8_t* data,
                     const size_t size,
                     bool (*string_to_num)(gurl_base::StringPiece, NumberType*)) {
  return CheckRoundtripsT<NumberType, gurl_base::StringPiece, std::string>(
      data, size, &gurl_base::NumberToString, string_to_num);
}

template <class NumberType>
void CheckRoundtrips16(const uint8_t* data,
                       const size_t size,
                       bool (*string_to_num)(gurl_base::StringPiece16,
                                             NumberType*)) {
  return CheckRoundtripsT<NumberType, gurl_base::StringPiece16, std::u16string>(
      data, size, &gurl_base::NumberToString16, string_to_num);
}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // For each instantiation of NumberToString f and its corresponding StringTo*
  // function g, check that f(g(x)) = x holds for fuzzer-determined values of x.
  CheckRoundtrips<int>(data, size, &gurl_base::StringToInt);
  CheckRoundtrips16<int>(data, size, &gurl_base::StringToInt);
  CheckRoundtrips<unsigned int>(data, size, &gurl_base::StringToUint);
  CheckRoundtrips16<unsigned int>(data, size, &gurl_base::StringToUint);
  CheckRoundtrips<int64_t>(data, size, &gurl_base::StringToInt64);
  CheckRoundtrips16<int64_t>(data, size, &gurl_base::StringToInt64);
  CheckRoundtrips<uint64_t>(data, size, &gurl_base::StringToUint64);
  CheckRoundtrips16<uint64_t>(data, size, &gurl_base::StringToUint64);
  CheckRoundtrips<size_t>(data, size, &gurl_base::StringToSizeT);
  CheckRoundtrips16<size_t>(data, size, &gurl_base::StringToSizeT);

  gurl_base::StringPiece string_piece_input(reinterpret_cast<const char*>(data),
                                       size);
  std::string string_input(reinterpret_cast<const char*>(data), size);

  int out_int;
  gurl_base::StringToInt(string_piece_input, &out_int);
  unsigned out_uint;
  gurl_base::StringToUint(string_piece_input, &out_uint);
  int64_t out_int64;
  gurl_base::StringToInt64(string_piece_input, &out_int64);
  uint64_t out_uint64;
  gurl_base::StringToUint64(string_piece_input, &out_uint64);
  size_t out_size;
  gurl_base::StringToSizeT(string_piece_input, &out_size);

  // Test for StringPiece16 if size is even.
  if (size % 2 == 0) {
    gurl_base::StringPiece16 string_piece_input16(
        reinterpret_cast<const char16_t*>(data), size / 2);

    gurl_base::StringToInt(string_piece_input16, &out_int);
    gurl_base::StringToUint(string_piece_input16, &out_uint);
    gurl_base::StringToInt64(string_piece_input16, &out_int64);
    gurl_base::StringToUint64(string_piece_input16, &out_uint64);
    gurl_base::StringToSizeT(string_piece_input16, &out_size);
  }

  double out_double;
  gurl_base::StringToDouble(string_input, &out_double);

  gurl_base::HexStringToInt(string_piece_input, &out_int);
  gurl_base::HexStringToUInt(string_piece_input, &out_uint);
  gurl_base::HexStringToInt64(string_piece_input, &out_int64);
  gurl_base::HexStringToUInt64(string_piece_input, &out_uint64);
  std::vector<uint8_t> out_bytes;
  gurl_base::HexStringToBytes(string_piece_input, &out_bytes);

  gurl_base::HexEncode(data, size);

  // Convert the numbers back to strings.
  gurl_base::NumberToString(out_int);
  gurl_base::NumberToString16(out_int);
  gurl_base::NumberToString(out_uint);
  gurl_base::NumberToString16(out_uint);
  gurl_base::NumberToString(out_int64);
  gurl_base::NumberToString16(out_int64);
  gurl_base::NumberToString(out_uint64);
  gurl_base::NumberToString16(out_uint64);
  gurl_base::NumberToString(out_double);
  gurl_base::NumberToString16(out_double);

  return 0;
}
