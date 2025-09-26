#include <limits.h>
#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/number_codec.hpp"
#include "hessian2/string_reader.hpp"
#include "hessian2/string_writer.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace Hessian2 {
class NumberCodecTest : public testing::Test {
 public:
  template <typename T>
  void decodeSucc(absl::string_view data, T out, size_t size) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<T>();
    EXPECT_EQ(out, *output);
    EXPECT_EQ(size, decoder.offset());
  }

  template <typename T>
  void decodeFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<T>();
    EXPECT_EQ(nullptr, output);
  }

  template <typename T>
  void encodeSucc(T data, size_t size, std::string expected_data = "") {
    std::string res;
    Hessian2::Encoder encoder(res);
    encoder.encode<T>(data);
    if (!expected_data.empty()) {
      EXPECT_EQ(expected_data, res);
    }
    decodeSucc<T>(res, data, size);
  }
};

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForDouble) {
  { EXPECT_TRUE(Decode<double>("replyDouble_0_0", 0.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_1_0", 1.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_2_0", 2.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_127_0", 127.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_m128_0", -128.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_128_0", 128.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_m129_0", -129.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_32767_0", 32767.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_m32768_0", -32768.0)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_0_001", 0.001)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_m0_001", -0.001)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_65_536", 65.536)); }
  { EXPECT_TRUE(Decode<double>("replyDouble_3_14159", 3.14159)); }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForDouble) {
  { EXPECT_TRUE(Encode<double>("argDouble_0_0", 0.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_1_0", 1.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_2_0", 2.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_127_0", 127.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_m128_0", -128.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_128_0", 128.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_m129_0", -129.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_32767_0", 32767.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_m32768_0", -32768.0)); }
  { EXPECT_TRUE(Encode<double>("argDouble_0_001", 0.001)); }
  { EXPECT_TRUE(Encode<double>("argDouble_m0_001", -0.001)); }
  { EXPECT_TRUE(Encode<double>("argDouble_65_536", 65.536)); }
  { EXPECT_TRUE(Encode<double>("argDouble_3_14159", 3.14159)); }
}

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForInteger) {
  { EXPECT_TRUE(Decode<int32_t>("replyInt_0", 0)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_1", 1)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_47", 47)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_m16", -16)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_0x30", 48)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_0x7ff", 2047)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_m17", -17)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_m0x800", -2048)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_0x800", 2048)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_0x3ffff", 262143)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_m0x801", -2049)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_m0x40000", -262144)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_0x40000", 262144)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_0x7fffffff", 2147483647)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_m0x40001", -262145)); }
  { EXPECT_TRUE(Decode<int32_t>("replyInt_m0x80000000", -2147483648)); }

  { EXPECT_TRUE(Decode<int64_t>("replyLong_0", 0)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_1", 1)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_15", 15)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m8", -8)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_0x10", 16)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_0x7ff", 2047)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m9", -9)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m0x800", -2048)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_0x800", 2048)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_0x3ffff", 262143)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m0x801", -2049)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m0x40000", -262144)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_0x40000", 262144)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_0x7fffffff", 2147483647)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m0x40001", -262145)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m0x80000000", -2147483648)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_0x80000000", 2147483648)); }
  { EXPECT_TRUE(Decode<int64_t>("replyLong_m0x80000001", -2147483649)); }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForInteger) {
  { EXPECT_TRUE(Encode<int32_t>("argInt_0", 0)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_1", 1)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_47", 47)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_m16", -16)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_0x30", 48)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_0x7ff", 2047)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_m17", -17)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_m0x800", -2048)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_0x800", 2048)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_0x3ffff", 262143)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_m0x801", -2049)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_m0x40000", -262144)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_0x40000", 262144)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_0x7fffffff", 2147483647)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_m0x40001", -262145)); }
  { EXPECT_TRUE(Encode<int32_t>("argInt_m0x80000000", -2147483648)); }

  { EXPECT_TRUE(Encode<int64_t>("argLong_0", 0)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_1", 1)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_15", 15)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m8", -8)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_0x10", 16)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_0x7ff", 2047)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m9", -9)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m0x800", -2048)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_0x800", 2048)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_0x3ffff", 262143)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m0x801", -2049)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m0x40000", -262144)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_0x40000", 262144)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_0x7fffffff", 2147483647)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m0x40001", -262145)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m0x80000000", -2147483648)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_0x80000000", 2147483648)); }
  { EXPECT_TRUE(Encode<int64_t>("argLong_m0x80000001", -2147483649)); }
}

TEST_F(NumberCodecTest, DecodeDouble) {
  {
    std::string data{};
    decodeFail<double>(data);
  }

  {
    std::string data{0x5d};
    decodeFail<double>(data);
  }

  {
    std::string data{0x5e};
    decodeFail<double>(data);
  }

  {
    std::string data{0x5f};
    decodeFail<double>(data);
  }

  {
    std::string data{0x44};
    decodeFail<double>(data);
  }

  {
    std::string data{0x01};
    decodeFail<double>(data);
  }

  {
    std::string data{0x5b};
    decodeSucc<double>(data, 0.0, 1);
  }

  {
    std::string data{0x5c};
    decodeSucc<double>(data, 1.0, 1);
  }

  {
    char buf[] = {0x5d, 0x00};
    std::string data(buf, 2);
    decodeSucc<double>(data, 0.0, 2);
  }

  {
    unsigned char buf[] = {0x5d, 0x80};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<double>(data, -128.0, 2);
  }

  {
    unsigned char buf[] = {0x5d, 0x7f};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<double>(data, 127.0, 2);
  }

  {
    unsigned char buf[] = {0x5e, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<double>(data, 0.0, 3);
  }

  {
    unsigned char buf[] = {0x5e, 0x80, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<double>(data, -32768.0, 3);
  }

  {
    unsigned char buf[] = {0x5e, 0x7f, 0xff};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<double>(data, 32767.0, 3);
  }

  {
    unsigned char buf[] = {0x5f, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 5);
    decodeSucc<double>(data, 0.0, 5);
  }

  {
    unsigned char buf[] = {
        0x44, 0x40, 0x28, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    std::string data(reinterpret_cast<char *>(buf), 9);
    decodeSucc<double>(data, 12.25, 9);
  }
}

TEST_F(NumberCodecTest, EncodeDouble) {
  {
    double val = 0;
    std::string data{0x5b};
    encodeSucc<double>(val, 1, data);
  }

  {
    double val = 1;
    std::string data{0x5c};
    encodeSucc<double>(val, 1, data);
  }

  {
    for (double val = -0x80; val < 0x80; val++) {
      if (val == 0 || val == 1) {
        continue;
      }
      encodeSucc<double>(val, 2);
    }
  }

  {
    for (double val = -0x8000; val < 0x8000; val++) {
      if (val >= -0x80 && val < 0x80) {
        continue;
      }
      encodeSucc<double>(val, 3);
    }
  }

  {
    unsigned char buf[] = {'D', 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 9);
    double val = 2.2250738585072014E-308;
    encodeSucc<double>(val, 9, data);
  }

  {
    unsigned char buf[] = {'D', 0x40, 0x28, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 9);
    double val = 12.25;
    encodeSucc<double>(val, 9, data);
  }

  {
    unsigned char buf[] = {'D', 0xc0, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 9);
    double val = -12.5;
    encodeSucc<double>(val, 9, data);
  }
}

TEST_F(NumberCodecTest, DecodeInt32) {
  // Insufficient data
  {
    std::string data;
    decodeFail<int32_t>(data);
  }

  {
    unsigned char buf[] = {0xc1};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int32_t>(data);
  }

  {
    unsigned char buf[] = {0xd0};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int32_t>(data);
  }

  {
    unsigned char buf[] = {0x49};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int32_t>(data);
  }
  // Incorrect type
  {
    unsigned char buf[] = {0x01};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int32_t>(data);
  }

  // Single octet integers
  {
    for (int8_t i = -16; i <= 47; ++i) {
      int32_t value = i;
      int8_t code = 0x90 + i;
      std::string data(reinterpret_cast<char *>(&code), 1);
      decodeSucc<int32_t>(data, value, 1);
    }
  }

  {
    unsigned char buf[] = {0x90};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeSucc<int32_t>(data, 0, 1);
  }

  {
    unsigned char buf[] = {0x80};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeSucc<int32_t>(data, -16, 1);
  }

  {
    unsigned char buf[] = {0xbf};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeSucc<int32_t>(data, 47, 1);
  }
  // Two octet integers
  {
    for (int32_t i = -2048; i < -16; ++i) {
      encodeSucc(i, 2);
    }

    for (int32_t i = 48; i < 2047; ++i) {
      encodeSucc(i, 2);
    }
  }

  {
    unsigned char buf[] = {0xc8, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int32_t>(data, 0, 2);
  }

  {
    unsigned char buf[] = {0xc0, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int32_t>(data, -2048, 2);
  }

  {
    unsigned char buf[] = {0xc7, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int32_t>(data, -256, 2);
  }

  {
    unsigned char buf[] = {0xcf, 0xff};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int32_t>(data, 2047, 2);
  }
  // Three octet integers
  {
    unsigned char buf[] = {0xd4, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<int32_t>(data, 0, 3);
  }

  {
    unsigned char buf[] = {0xd0, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<int32_t>(data, -262144, 3);
  }

  {
    unsigned char buf[] = {0xd7, 0xff, 0xff};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<int32_t>(data, 262143, 3);
  }
  // Four octet integers
  {
    unsigned char buf[] = {0x49, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 5);
    decodeSucc<int32_t>(data, 0, 5);
  }

  {
    unsigned char buf[] = {0x49, 0x00, 0x00, 0x01, 0x2c};
    std::string data(reinterpret_cast<char *>(buf), 5);
    decodeSucc<int32_t>(data, 300, 5);
  }
}

TEST_F(NumberCodecTest, EncodeInt32) {
  // Single octet integers
  {
    int32_t val = 0;
    std::string data{'\x90'};
    encodeSucc<int32_t>(val, 1, data);
  }

  {
    int32_t val = -16;
    std::string data{'\x80'};
    encodeSucc<int32_t>(val, 1, data);
  }

  {
    int32_t val = 47;
    std::string data{'\xbf'};
    encodeSucc<int32_t>(val, 1, data);
  }

  {
    for (int32_t val = -16; val < 48; val++) {
      encodeSucc<int32_t>(val, 1);
    }
  }
  // Two octet integers
  {
    int32_t val = -2048;
    std::string data{'\xc0', 0x00};
    encodeSucc<int32_t>(val, 2, data);
  }

  {
    int32_t val = -256;
    std::string data{'\xc7', 0x00};
    encodeSucc<int32_t>(val, 2, data);
  }

  {
    int32_t val = 2047;
    std::string data{'\xcf', '\xff'};
    encodeSucc<int32_t>(val, 2, data);
  }

  {
    for (int32_t val = -2048; val < 2048; val++) {
      if (val >= -16 && val < 48) {
        continue;
      }
      encodeSucc<int32_t>(val, 2);
    }
  }
  // Three octet integers
  {
    int32_t val = -262144;
    std::string data{'\xd0', 0x00, 0x00};
    encodeSucc<int32_t>(val, 3, data);
  }

  {
    int32_t val = 262143;
    std::string data{'\xd7', '\xff', '\xff'};
    encodeSucc<int32_t>(val, 3, data);
  }

  {
    int32_t val = 2048;
    std::string data{'\xd4', '\b', '\x00'};
    encodeSucc<int32_t>(val, 3, data);
  }

  {
    for (int32_t val = -262144; val < 262144; val++) {
      if (val >= -2048 && val < 2048) {
        continue;
      }
      encodeSucc<int32_t>(val, 3);
    }
  }
  // Five octet integers
  {
    std::string data{0x49, '\x7f', '\xff', '\xff', '\xff'};
    encodeSucc<int32_t>(INT32_MAX, 5, data);
  }

  {
    std::string data{0x49, '\x80', '\x00', '\x00', '\x00'};
    encodeSucc<int32_t>(INT32_MIN, 5, data);
  }
}

TEST_F(NumberCodecTest, DecodeInt64) {
  // Insufficient data
  {
    std::string data;
    decodeFail<int64_t>(data);
  }

  {
    unsigned char buf[] = {0xf0};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int64_t>(data);
  }

  {
    unsigned char buf[] = {0x38, '1'};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int64_t>(data);
  }

  {
    unsigned char buf[] = {0x59, '1'};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int64_t>(data);
  }

  {
    unsigned char buf[] = {0x4c, '1'};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int64_t>(data);
  }
  // Incorrect type
  {
    unsigned char buf[] = {0x40};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeFail<int64_t>(data);
  }
  // Single octet longs
  {
    for (int64_t i = -8, code = 0xd8; i <= 15 && code <= 0xef; ++i, ++code) {
      std::string data(reinterpret_cast<char *>(&code), 1);
      decodeSucc<int64_t>(data, i, 1);
    }
  }

  {
    unsigned char buf[] = {0xe0};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeSucc<int64_t>(data, 0, 1);
  }

  {
    unsigned char buf[] = {0xd8};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeSucc<int64_t>(data, -8, 1);
  }

  {
    unsigned char buf[] = {0xef};
    std::string data(reinterpret_cast<char *>(buf), 1);
    decodeSucc<int64_t>(data, 15, 1);
  }
  // Two octet longs
  {
    unsigned char buf[] = {0xf8, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int64_t>(data, 0, 2);
  }

  {
    unsigned char buf[] = {0xf0, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int64_t>(data, -2048, 2);
  }

  {
    unsigned char buf[] = {0xf7, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int64_t>(data, -256, 2);
  }

  {
    unsigned char buf[] = {0xff, 0xff};
    std::string data(reinterpret_cast<char *>(buf), 2);
    decodeSucc<int64_t>(data, 2047, 2);
  }
  // Three octet longs
  {
    unsigned char buf[] = {0x3c, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<int64_t>(data, 0, 3);
  }

  {
    unsigned char buf[] = {0x38, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<int64_t>(data, -262144, 3);
  }

  {
    unsigned char buf[] = {0x3f, 0xff, 0xff};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc<int64_t>(data, 262143, 3);
  }
  // Four octet longs
  {
    unsigned char buf[] = {0x59, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 5);
    decodeSucc<int64_t>(data, 0, 5);
  }

  {
    unsigned char buf[] = {0x59, 0x00, 0x00, 0x01, 0x2c};
    std::string data(reinterpret_cast<char *>(buf), 5);
    decodeSucc<int64_t>(data, 300, 5);
  }
  // eight octet longs
  {
    unsigned char buf[] = {0x4c, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x01, 0x2c};
    std::string data(reinterpret_cast<char *>(buf), 9);
    decodeSucc<int64_t>(data, 300, 9);
  }
}

TEST_F(NumberCodecTest, EncodeInt64) {
  // Single octet longs
  {
    int64_t val = 0;
    std::string data{'\xe0'};
    encodeSucc<int64_t>(val, 1, data);
  }

  {
    int64_t val = -8;
    std::string data{'\xd8'};
    encodeSucc<int64_t>(val, 1, data);
  }

  {
    int64_t val = 15;
    std::string data{'\xef'};
    encodeSucc<int64_t>(val, 1, data);
  }

  {
    for (int64_t val = -8; val < 16; val++) {
      encodeSucc<int64_t>(val, 1);
    }
  }
  // Two octet longs
  {
    int64_t val = -2048;
    std::string data{'\xf0', 0x00};
    encodeSucc<int64_t>(val, 2, data);
  }

  {
    int64_t val = -256;
    std::string data{'\xf7', 0x00};
    encodeSucc<int64_t>(val, 2, data);
  }

  {
    int32_t val = 2047;
    std::string data{'\xff', '\xff'};
    encodeSucc<int64_t>(val, 2, data);
  }

  {
    for (int64_t val = -2048; val < 2048; val++) {
      if (val >= -8 && val < 16) {
        continue;
      }
      encodeSucc<int64_t>(val, 2);
    }
  }
  // Three octet longs
  {
    int64_t val = -262144;
    std::string data{0x38, 0x00, 0x00};
    encodeSucc<int64_t>(val, 3, data);
  }

  {
    int64_t val = 262143;
    std::string data{0x3f, '\xff', '\xff'};
    encodeSucc<int64_t>(val, 3, data);
  }

  {
    int64_t val = 2048;
    std::string data{0x3c, '\b', '\x00'};
    encodeSucc<int64_t>(val, 3, data);
  }

  {
    for (int64_t val = -262144; val < 262144; val++) {
      if (val >= -2048 && val < 2048) {
        continue;
      }
      encodeSucc<int64_t>(val, 3);
    }
  }
  // Five octet integers
  {
    std::string data{0x59, '\x7f', '\xff', '\xff', '\xff'};
    encodeSucc<int64_t>(INT32_MAX, 5, data);
  }

  {
    std::string data{0x59, '\x80', '\x00', '\x00', '\x00'};
    encodeSucc<int64_t>(INT32_MIN, 5, data);
  }
  // Eight octet integers
  {
    std::string data{0x4c,   '\x7f', '\xff', '\xff', '\xff',
                     '\xff', '\xff', '\xff', '\xff'};
    encodeSucc<int64_t>(INT64_MAX, 9, data);
  }

  {
    std::string data{0x4c,   '\x80', '\x00', '\x00', '\x00',
                     '\x00', '\x00', '\x00', '\x00'};
    encodeSucc<int64_t>(INT64_MIN, 9, data);
  }
}

}  // namespace Hessian2
