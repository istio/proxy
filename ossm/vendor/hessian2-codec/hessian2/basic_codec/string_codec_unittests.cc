#include <iostream>
#include <type_traits>

#include "common/common.h"
#include "gtest/gtest.h"
#include "hessian2/basic_codec/string_codec.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace {
std::string GenerateString1023() {
  std::string expect;
  for (int i = 0; i < 16; ++i) {
    expect.append(absl::StrFormat(
        "%d%d 456789012345678901234567890123456789012345678901234567890123\n",
        i / 10, i % 10));
  }
  expect.resize(1023);
  return expect;
}

std::string GenerateString1024() {
  std::string expect;
  for (int i = 0; i < 16; ++i) {
    expect.append(absl::StrFormat(
        "%d%d 456789012345678901234567890123456789012345678901234567890123\n",
        i / 10, i % 10));
  }

  expect.resize(1024);
  return expect;
}

std::string GenerateString65536() {
  std::string expect;
  for (int i = 0; i < 1024; ++i) {
    expect.append(absl::StrFormat(
        "%d%d%d 56789012345678901234567890123456789012345678901234567890123\n",
        i / 100, i / 10 % 10, i % 10));
  }
  expect.resize(65536);
  return expect;
}

std::string GenerateString131072() {
  std::string expect;
  for (int i = 0; i < 3072; ++i) {
    expect.append(absl::StrFormat(
        "%d%d%d 56789012345678901234567890123456789012345678901234567890123\n",
        i / 100, i / 10 % 10, i % 10));
  }

  expect.resize(131072);
  return expect;
}

std::string GenerateEmojiString() {
  uint32_t emoji = 0x0001f923;
  uint32_t max_unicode = 0x0010ffff;

  std::string s;

  // Write the first emoji codepoint as a UTF-8 string.
  s.push_back(0xf0 | (emoji >> 18));
  s.push_back(0x80 | ((emoji >> 12) & 0x3f));
  s.push_back(0x80 | ((emoji >> 6) & 0x3f));
  s.push_back(0x80 | (emoji & 0x3f));

  s += ",max";

  // Write the max unicode codepoint as a UTF-8 string.
  s.push_back(0xf0 | (max_unicode >> 18));
  s.push_back(0x80 | ((max_unicode >> 12) & 0x3f));
  s.push_back(0x80 | ((max_unicode >> 6) & 0x3f));
  s.push_back(0x80 | (max_unicode & 0x3f));

  return s;
}

std::string GenerateComplexString() {
  return "킐\u0088中国你好!\u0088\u0088\u0088\u0088\u0088\u0088";
}

std::string GenerateSuperComplexString() {
  return "킐\u0088中国你好!"
         "\u0088\u0088\u0088\u0088\u0088\u0088✅❓☑️😊🤔👀🫅🔒🗝️🧫🛹🚅"
         "🧻🪞🪞🪞🪞"
         "🪞🪞🪞🪞🪞🕟🕟🕟🕟🕟🕟🕟🔅🔅🔅🔅🔅🔅🤍🤍🤍🤍🤍🤍🌈🌈🌈🌈🌈🌈🏦🏦🏦"
         "🏦"
         "🏦🏦🚎🚎🚎🚎🚎🚎🚎⏰⏰⏰⏰⏰⏲️⏲️⏲️🗄️abcdefghijklmnopqrstuvwxyz1234567@#"
         "$"
         "%^&*()_+⏲️⏲️⏲️⏲️🐪🐫c⏰";
}

}  // namespace

namespace Hessian2 {
class StringCodecTest : public testing::Test {
 public:
  void decodeSucc(absl::string_view data, std::string out, size_t size) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<std::string>();
    EXPECT_EQ(out, *output);
    EXPECT_EQ(size, decoder.offset());
  }

  void decodeFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<std::string>();
    EXPECT_EQ(nullptr, output);
  }

  void encodeSucc(std::string data, size_t size,
                  std::string expected_data = "") {
    std::string res;
    Hessian2::Encoder encoder(res);
    encoder.encode<std::string>(data);
    if (!expected_data.empty()) {
      EXPECT_EQ(expected_data, res);
    }
    decodeSucc(res, data, size);
  }
};

TEST(SimpleDecodingAndEncodingTest, SimpleDecodingAndEncodingTest) {
  {
    std::string buffer;

    std::string value = GenerateString131072();

    Hessian2::Encoder encoder(buffer);

    encoder.encode(value);

    Hessian2::Decoder decoder(buffer);

    EXPECT_EQ(*decoder.decode<std::string>(), value);
  }
}

TEST(EmojiDecodingAndEncodingTest, EmojiDecodingAndEncodingTest) {
  {
    std::string buffer;

    std::string value = GenerateEmojiString();

    Hessian2::Encoder encoder(buffer);

    encoder.encode(value);

    Hessian2::Decoder decoder(buffer);

    EXPECT_EQ(*decoder.decode<std::string>(), value);
  }
}

TEST(ComplexDecodingAndEncodingTest, ComplexDecodingAndEncodingTest) {
  {
    std::string buffer;

    std::string value = GenerateComplexString() + GenerateSuperComplexString() +
                        GenerateString131072() + GenerateSuperComplexString() +
                        GenerateComplexString() + GenerateSuperComplexString();

    Hessian2::Encoder encoder(buffer);

    encoder.encode(value);

    Hessian2::Decoder decoder(buffer);

    EXPECT_EQ(*decoder.decode<std::string>(), value);
  }
}

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForString) {
  { EXPECT_TRUE(Decode<std::string>("replyString_0", std::string())); }
  { EXPECT_TRUE(Decode<std::string>("replyString_1", std::string({'0'}))); }
  {
    EXPECT_TRUE(Decode<std::string>(
        "replyString_31", std::string("0123456789012345678901234567890")));
  }
  {
    EXPECT_TRUE(Decode<std::string>(
        "replyString_32", std::string("01234567890123456789012345678901")));
  }
  {
    EXPECT_TRUE(Decode<std::string>("replyString_1023", GenerateString1023()));
  }

  {
    EXPECT_TRUE(Decode<std::string>("replyString_1024", GenerateString1024()));
  }

  {
    EXPECT_TRUE(
        Decode<std::string>("replyString_65536", GenerateString65536()));
  }

  {
    EXPECT_TRUE(Decode<std::string>("customReplyComplexString",
                                    GenerateComplexString()));
  }

  {
#ifdef COMPATIBLE_WITH_JAVA_HESSIAN_LITE
    EXPECT_TRUE(Decode<std::string>("customReplySuperComplexString",
                                    GenerateSuperComplexString()));

#endif
  }

  {
#ifdef COMPATIBLE_WITH_JAVA_HESSIAN_LITE
    EXPECT_TRUE(
        Decode<std::string>("customReplyStringEmoji", GenerateEmojiString()));
#endif
  }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForString) {
  { EXPECT_TRUE(Encode<std::string>("argString_0", std::string())); }
  { EXPECT_TRUE(Encode<std::string>("argString_1", std::string({'0'}))); }

  {
    EXPECT_TRUE(Encode<std::string>(
        "argString_31", std::string("0123456789012345678901234567890")));
  }

  {
    EXPECT_TRUE(Encode<std::string>(
        "argString_32", std::string("01234567890123456789012345678901")));
  }

  { EXPECT_TRUE(Encode<std::string>("argString_1023", GenerateString1023())); }
  { EXPECT_TRUE(Encode<std::string>("argString_1024", GenerateString1024())); }
  {
    EXPECT_TRUE(Encode<std::string>("argString_65536", GenerateString65536()));
  }

  {
    EXPECT_TRUE(
        Encode<std::string>("customArgComplexString", GenerateComplexString()));
  }
}

TEST_F(StringCodecTest, Decode) {
  // Insufficient data
  {
    std::string data{0x01};
    decodeFail(data);
  }

  {
    std::string data{0x30};
    decodeFail(data);
  }

  {
    std::string data{0x30, 't'};
    decodeFail(data);
  }

  {
    std::string data{0x53, 't'};
    decodeFail(data);
  }

  {
    std::string data{0x53, 't', 'e'};
    decodeFail(data);
  }

  {
    std::string data{0x52, 't'};
    decodeFail(data);
  }

  // Incorrect type
  {
    std::string data{0x20, 't'};
    decodeFail(data);
  }

  {
    std::string data{0x01, 't'};
    decodeSucc(data, "t", 2);
  }

  // empty string
  {
    std::string data{0x00};
    decodeSucc(data, "", 1);
  }

  {
    std::string data{0x01, 0x00};
    std::string str;
    str.push_back('\0');
    decodeSucc(data, str, 2);
  }

  {
    unsigned char buf[] = {0x01, 0xc3, 0x83};
    std::string data(reinterpret_cast<char *>(buf), 3);
    decodeSucc(data, "Ã", 3);
  }

  {
    // utf-8 encode character "中文"
    unsigned char buf[] = {0x02, 0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87};
    std::string data(reinterpret_cast<char *>(buf), 7);
    decodeSucc(data, "中文", 7);
  }

  {
    std::string data{0x53, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o'};
    decodeSucc(data, "hello", 8);
  }

  {
    std::string data{0x05, 'h', 'e', 'l', 'l', 'o'};
    decodeSucc(data, "hello", 6);
  }

  {
    std::string data{0x52, 0x00, 0x07, 'h', 'e', 'l', 'l', 'o',
                     ',',  ' ',  0x05, 'w', 'o', 'r', 'l', 'd'};
    decodeSucc(data, "hello, world", 16);
  }

  {
    std::string expect_string(257, 't');
    std::string prefix{0x31, 0x01};
    std::string data = prefix.append(expect_string);
    decodeSucc(data, expect_string, 259);
  }
}

TEST_F(StringCodecTest, Encode) {
  // empty string

  {
    std::string input;
    encodeSucc(input, 1);
  }

  {
    std::string input{0x00};
    encodeSucc(input, 2);
  }

  {
    // utf-8 encode character "中文"
    std::string input(u8"中文");
    unsigned char buf[] = {0x02, 0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87};
    std::string expect(reinterpret_cast<char *>(buf), sizeof(buf));
    encodeSucc(input, 7, expect);
  }

  {
    std::string input("hello");
    encodeSucc(input, 6);
  }

  {
    std::string input("hello, world");
    encodeSucc(input, 13);
  }

  {
    std::string input(257, 't');
    encodeSucc(input, 259);
  }

  {
    std::string test_str(32, 't');
    std::string expect_str = "\x30\x20" + test_str;
    encodeSucc(test_str, 34, expect_str);
  }

  {
    std::string input(256, 't');
    std::string expect_str = std::string{0x31, 0x00} + input;
    encodeSucc(input, 258, expect_str);
  }

  {
    std::string input(1024, 't');
    std::string expect_str = std::string{'S', 0x04, 0x00} + input;
    encodeSucc(input, 1027, expect_str);
  }

  {
    std::string input(65536, 't');
    std::string expect_str = "\x52\x80" + std::string(1, '\0') +
                             std::string(32768, 't') + "\x53\x80" +
                             std::string(1, '\0') + std::string(32768, 't');
    encodeSucc(input, 65542, expect_str);
  }

  {
    std::string input(u8"🤣🤣🤣");
#ifdef COMPATIBLE_WITH_JAVA_HESSIAN_LITE
    encodeSucc(input, 19);  // 1 byte for length, 18 bytes for data.
#else
    encodeSucc(input, 13);  // 1 byte for length, 12 bytes for data.
#endif
  }
}

}  // namespace Hessian2
