#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/bool_codec.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace Hessian2 {
class BoolCodecTest : public testing::Test {
 public:
  void decodeSucc(absl::string_view data, bool out) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<bool>();
    EXPECT_EQ(out, *output);
    EXPECT_EQ(1, decoder.offset());
  }

  void decodeFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<bool>();
    EXPECT_EQ(nullptr, output);
  }

  void encodeSucc(bool data, std::string expected_data = "") {
    std::string res;
    Hessian2::Encoder encoder(res);
    encoder.encode<bool>(data);
    if (!expected_data.empty()) {
      EXPECT_EQ(expected_data, res);
    }
    decodeSucc(res, data);
  }
};

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForBool) {
  { EXPECT_TRUE(Decode<bool>("replyTrue", true)); }
  { EXPECT_TRUE(Decode<bool>("replyFalse", false)); }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForBool) {
  { EXPECT_TRUE(Encode<bool>("argTrue", true)); }
  { EXPECT_TRUE(Encode<bool>("argFalse", false)); }
}

TEST_F(BoolCodecTest, Decode) {
  {
    std::string data{0x01};
    decodeFail(data);
  }

  {
    std::string data{0x00};
    decodeFail(data);
  }

  {
    std::string data{'F'};
    decodeSucc(data, false);
  }

  {
    std::string data{'T'};
    decodeSucc(data, true);
  }

  {
    std::string data{'F', 'T'};
    decodeSucc(data, false);
  }
}

TEST_F(BoolCodecTest, encode) {
  { encodeSucc(true, "T"); }

  { encodeSucc(false, "F"); }
}

}  // namespace Hessian2
