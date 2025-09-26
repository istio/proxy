#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/def_ref_codec.hpp"
#include "hessian2/object.hpp"

namespace Hessian2 {

class DefinitionTest : public testing::Test {
 public:
  void decodeSucc(absl::string_view data, Object::Definition out) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<Object::Definition>();
    EXPECT_EQ(out, *output);
    EXPECT_EQ(1, decoder.offset());
  }

  void decodeFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<Object::Definition>();
    EXPECT_EQ(nullptr, output);
  }
};

// Insufficient Data
TEST_F(DefinitionTest, InsufficientData) {
  {
    std::string data;
    decodeFail(data);
  }

  {
    std::string data{'C'};
    decodeFail(data);
  }

  {
    std::string data{'C', 0x05, 'h', 'e'};
    decodeFail(data);
  }
}

TEST(DefRefCodecTest, Decode) {
  {
    unsigned char buf[] = {'C',  0x05, 'h', 'e', 'l', 'l',  'o',  0x92,
                           0x05, 'c',  'o', 'l', 'o', 'r',  0x05, 'm',
                           'o',  'd',  'e', 'l', 'O', 0x90, 0x60, 0x61};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Hessian2::Decoder decoder(data);
    Object::RawDefinition def;
    def.type_ = "hello";
    def.field_names_.push_back("color");
    def.field_names_.push_back("model");

    auto output = decoder.decode<Object::Definition>();
    EXPECT_EQ(def, *output->data_);
    EXPECT_EQ(1, decoder.getDefRefSize());

    auto output2 = decoder.decode<Object::Definition>();
    EXPECT_EQ(def, *output2->data_);
    EXPECT_EQ(output->data_, output2->data_);
    EXPECT_EQ(1, decoder.getDefRefSize());

    auto output3 = decoder.decode<Object::Definition>();
    EXPECT_EQ(def, *output3->data_);
    EXPECT_EQ(output3->data_, output2->data_);
    EXPECT_EQ(output3->data_, output->data_);
    EXPECT_EQ(1, decoder.getDefRefSize());

    auto output4 = decoder.decode<Object::Definition>();
    EXPECT_TRUE(output4 == nullptr);
  }
}

TEST(DefRefCodecTest, encode) {
  {
    unsigned char buf[] = {'C',  0x05, 'h', 'e', 'l', 'l', 'o',
                           0x92, 0x05, 'c', 'o', 'l', 'o', 'r',
                           0x05, 'm',  'o', 'd', 'e', 'l', 0x60};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::RawDefinition def;
    def.type_ = "hello";
    def.field_names_.push_back("color");
    def.field_names_.push_back("model");
    std::string output;
    Hessian2::Encoder encoder(output);

    encoder.encode(def);
    EXPECT_EQ(1, encoder.getDefRefSize());
    EXPECT_EQ(data, output);
  }
}

}  // namespace Hessian2
