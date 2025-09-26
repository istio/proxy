// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

std::string output_std_string;
std::wstring output_std_wstring;
std::u16string output_string16;

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  gurl_base::StringPiece string_piece_input(reinterpret_cast<const char*>(data),
                                       size);

  std::ignore = gurl_base::UTF8ToWide(string_piece_input);
  gurl_base::UTF8ToWide(reinterpret_cast<const char*>(data), size,
                   &output_std_wstring);
  std::ignore = gurl_base::UTF8ToUTF16(string_piece_input);
  gurl_base::UTF8ToUTF16(reinterpret_cast<const char*>(data), size,
                    &output_string16);

  // Test for char16_t.
  if (size % 2 == 0) {
    gurl_base::StringPiece16 string_piece_input16(
        reinterpret_cast<const char16_t*>(data), size / 2);
    std::ignore = gurl_base::UTF16ToWide(output_string16);
    gurl_base::UTF16ToWide(reinterpret_cast<const char16_t*>(data), size / 2,
                      &output_std_wstring);
    std::ignore = gurl_base::UTF16ToUTF8(string_piece_input16);
    gurl_base::UTF16ToUTF8(reinterpret_cast<const char16_t*>(data), size / 2,
                      &output_std_string);
  }

  // Test for wchar_t.
  size_t wchar_t_size = sizeof(wchar_t);
  if (size % wchar_t_size == 0) {
    std::ignore = gurl_base::WideToUTF8(output_std_wstring);
    gurl_base::WideToUTF8(reinterpret_cast<const wchar_t*>(data),
                     size / wchar_t_size, &output_std_string);
    std::ignore = gurl_base::WideToUTF16(output_std_wstring);
    gurl_base::WideToUTF16(reinterpret_cast<const wchar_t*>(data),
                      size / wchar_t_size, &output_string16);
  }

  // Test for ASCII. This condition is needed to avoid hitting instant GURL_CHECK
  // failures.
  if (gurl_base::IsStringASCII(string_piece_input)) {
    output_string16 = gurl_base::ASCIIToUTF16(string_piece_input);
    gurl_base::StringPiece16 string_piece_input16(output_string16);
    std::ignore = gurl_base::UTF16ToASCII(string_piece_input16);
  }

  return 0;
}
