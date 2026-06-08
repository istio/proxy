#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/byte_codec.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace Hessian2 {
class ByteCodecTest : public testing::Test {
 public:
  void decodeSucc(absl::string_view data, std::vector<uint8_t> out,
                  size_t size) {
    Hessian2::Decoder decoder(data);
    std::unique_ptr<std::vector<uint8_t>> output =
        decoder.decode<std::vector<uint8_t>>();
    EXPECT_EQ(out.size(), output->size());
    EXPECT_EQ(out, *output);
    EXPECT_EQ(size, decoder.offset());
  }

  void decodeFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<std::vector<uint8_t>>();
    EXPECT_EQ(nullptr, output);
  }

  void encodeSucc(std::vector<uint8_t> data, size_t size,
                  std::string expected_data = "") {
    std::string res;
    Hessian2::Encoder encoder(res);
    encoder.encode<std::vector<uint8_t>>(data);
    if (!expected_data.empty()) {
      EXPECT_EQ(expected_data, res);
    }
    decodeSucc(res, data, size);
  }
};

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForByte) {
  {
    EXPECT_TRUE(
        Decode<std::vector<uint8_t>>("replyBinary_0", std::vector<uint8_t>{}));
  }
  {
    EXPECT_TRUE(Decode<std::vector<uint8_t>>("replyBinary_1",
                                             std::vector<uint8_t>{'0'}));
  }
  {
    std::string str("012345678901234");
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Decode<std::vector<uint8_t>>("replyBinary_15", vec));
  }
  {
    std::string str("0123456789012345");
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Decode<std::vector<uint8_t>>("replyBinary_16", vec));
  }
  {
    std::string str;
    for (int i = 0; i < 16; ++i) {
      str.append(std::to_string(i / 10));
      str.append(std::to_string(i % 10));
      str.append(
          " 456789012345678901234567890123456789012345678901234567890123\n");
    }
    str.resize(1023);
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Decode<std::vector<uint8_t>>("replyBinary_1023", vec));
  }
  {
    std::string str;
    for (int i = 0; i < 16; ++i) {
      str.append(std::to_string(i / 10));
      str.append(std::to_string(i % 10));
      str.append(
          " 456789012345678901234567890123456789012345678901234567890123\n");
    }
    str.resize(1024);
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Decode<std::vector<uint8_t>>("replyBinary_1024", vec));
  }
  {
    std::string str;
    for (int i = 0; i < 1024; ++i) {
      str.append(std::to_string(i / 100));
      str.append(std::to_string((i / 10) % 10));
      str.append(std::to_string(i % 10));
      str.append(
          " 56789012345678901234567890123456789012345678901234567890123\n");
    }
    str.resize(65536);
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Decode<std::vector<uint8_t>>("replyBinary_65536", vec));
  }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForByte) {
  {
    EXPECT_TRUE(
        Encode<std::vector<uint8_t>>("argBinary_0", std::vector<uint8_t>{}));
  }
  {
    EXPECT_TRUE(
        Encode<std::vector<uint8_t>>("argBinary_1", std::vector<uint8_t>{'0'}));
  }
  {
    std::string str("012345678901234");
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Encode<std::vector<uint8_t>>("argBinary_15", vec));
  }
  {
    std::string str("0123456789012345");
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Encode<std::vector<uint8_t>>("argBinary_16", vec));
  }
  {
    std::string str;
    for (int i = 0; i < 16; ++i) {
      str.append(std::to_string(i / 10));
      str.append(std::to_string(i % 10));
      str.append(
          " 456789012345678901234567890123456789012345678901234567890123\n");
    }
    str.resize(1023);
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Encode<std::vector<uint8_t>>("argBinary_1023", vec));
  }
  {
    std::string str;
    for (int i = 0; i < 16; ++i) {
      str.append(std::to_string(i / 10));
      str.append(std::to_string(i % 10));
      str.append(
          " 456789012345678901234567890123456789012345678901234567890123\n");
    }
    str.resize(1024);
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Encode<std::vector<uint8_t>>("argBinary_1024", vec));
  }
  {
    std::string str;
    for (int i = 0; i < 1024; ++i) {
      str.append(std::to_string(i / 100));
      str.append(std::to_string((i / 10) % 10));
      str.append(std::to_string(i % 10));
      str.append(
          " 56789012345678901234567890123456789012345678901234567890123\n");
    }
    str.resize(65536);
    std::vector<uint8_t> vec(str.begin(), str.end());
    EXPECT_TRUE(Encode<std::vector<uint8_t>>("argBinary_65536", vec));
  }
}

TEST_F(ByteCodecTest, Decode) {
  // Insufficient Data
  {
    std::string data;
    decodeFail(data);
  }

  {
    std::string data{0x41};
    decodeFail(data);
  }

  {
    std::string data{0x42};
    decodeFail(data);
  }

  {
    std::string data{0x19};
    decodeFail(data);
  }

  {
    for (uint8_t i = 0x21; i <= 0xef; ++i) {
      std::string data{static_cast<int8_t>(i)};
      decodeFail(data);
    }

    for (uint8_t i = 0x34; i <= 0x37; ++i) {
      std::string data{static_cast<int8_t>(i)};
      decodeFail(data);
    }
  }

  {
    std::string data{0x20};
    decodeSucc(data, std::vector<uint8_t>{}, 1);
  }

  {
    std::string data{0x23, 0x01, 0x02, 0x03};
    decodeSucc(data, std::vector<uint8_t>{1, 2, 3}, 4);
  }

  {
    std::vector<uint8_t> expect_output;
    std::string expect_string = {0x37, 0x10};
    expect_string.append(std::string(784, 't'));
    expect_output.insert(expect_output.begin(), expect_string.begin() + 2,
                         expect_string.end());
    decodeSucc(expect_string, expect_output, 786);
  }

  {
    std::vector<uint8_t> expect_output;
    unsigned char buf[] = {0x37, 0xFF};
    std::string expect_string(reinterpret_cast<char *>(&buf), 2);
    expect_string.append(std::string(1023, 't'));
    expect_output.insert(expect_output.begin(), expect_string.begin() + 2,
                         expect_string.end());
    decodeSucc(expect_string, expect_output, 1025);
  }

  {
    std::vector<uint8_t> expect_output;
    std::string expect_string = {0x42, 0x10, 0x00};
    expect_string.append(std::string(4096, 't'));
    expect_output.insert(expect_output.begin(), expect_string.begin() + 3,
                         expect_string.end());
    decodeSucc(expect_string, expect_output, 4099);
  }
}

TEST_F(ByteCodecTest, encode) {
  {
    std::string expect_string{0x23, 0x01, 0x02, 0x03};
    encodeSucc(std::vector<uint8_t>{0x01, 0x02, 0x03}, 4, expect_string);
  }

  {
    std::vector<uint8_t> data;
    std::string expect_string = {0x37, 0x10};
    expect_string.append(std::string(784, 't'));
    data.insert(data.begin(), expect_string.begin() + 2, expect_string.end());
    encodeSucc(data, 786, expect_string);
  }
}

}  // namespace Hessian2
