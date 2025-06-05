#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/date_codec.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace Hessian2 {
class DateCodecTest : public testing::Test {
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

  template <typename T>
  void expectedEncode(T data) {
    std::string res;
    Hessian2::Encoder encoder(res);
    encoder.encode<T>(data);

    Hessian2::Decoder decoder(res);
    auto value = decoder.decode<T>();
    EXPECT_EQ(*value, data);
  }
};

TEST_F(DateCodecTest, decode) {
  {
    expectedEncode(std::chrono::milliseconds(894621091000));
    expectedEncode(std::chrono::seconds(894621091));
    expectedEncode(std::chrono::hours(89462));
  }

  // Insufficient Data
  {
    std::string data{0x4a, 0x00, 0x00, 0x00};
    decodeFail<std::chrono::milliseconds>(data);
  }

  {
    std::string data{0x4b, 0x00, 0x00, 0x00};
    decodeFail<std::chrono::milliseconds>(data);
  }
}

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForDate) {
  {
    EXPECT_TRUE(Decode<std::chrono::milliseconds>(
        "replyDate_0", std::chrono::milliseconds(0)));
  }
  {
    EXPECT_TRUE(
        Decode<std::chrono::minutes>("replyDate_0", std::chrono::minutes(0)));
  }
  {
    EXPECT_TRUE(Decode<std::chrono::milliseconds>(
        "replyDate_1", std::chrono::milliseconds(894621091000)));
  }
  {
    EXPECT_TRUE(Decode<std::chrono::seconds>("replyDate_1",
                                             std::chrono::seconds(894621091)));
  }
  {
    EXPECT_TRUE(Decode<std::chrono::minutes>("replyDate_1",
                                             std::chrono::minutes(14910351)));
  }
  {
    EXPECT_TRUE(Decode<std::chrono::milliseconds>(
        "replyDate_2", std::chrono::milliseconds(894621060000)));
  }
  {
    EXPECT_TRUE(Decode<std::chrono::seconds>("replyDate_2",
                                             std::chrono::seconds(894621060)));
  }
  {
    EXPECT_TRUE(Decode<std::chrono::minutes>("replyDate_2",
                                             std::chrono::minutes(14910351)));
  }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForDate) {
  {
    EXPECT_TRUE(Encode<std::chrono::milliseconds>(
        "argDate_0", std::chrono::milliseconds(0)));
  }
  {
    EXPECT_TRUE(
        Encode<std::chrono::minutes>("argDate_0", std::chrono::minutes(0)));
  }
  {
    EXPECT_TRUE(Encode<std::chrono::milliseconds>(
        "argDate_1", std::chrono::milliseconds(894621091000)));
  }
  {
    EXPECT_TRUE(Encode<std::chrono::seconds>("argDate_1",
                                             std::chrono::seconds(894621091)));
  }
  {
    EXPECT_TRUE(Encode<std::chrono::milliseconds>(
        "argDate_2", std::chrono::milliseconds(894621060000)));
  }
  {
    EXPECT_TRUE(Encode<std::chrono::seconds>("argDate_2",
                                             std::chrono::seconds(894621060)));
  }
  {
    EXPECT_TRUE(Encode<std::chrono::minutes>("argDate_2",
                                             std::chrono::minutes(14910351)));
  }
}

}  // namespace Hessian2
