#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/type_ref_codec.hpp"
#include "hessian2/object.hpp"

namespace Hessian2 {

TEST(TypeRefCodecTest, Decode) {
  {
    std::string data{0x53, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o'};
    Hessian2::Decoder decoder(data);
    Object::TypeRef ref("hello");
    auto output = decoder.decode<Object::TypeRef>();
    EXPECT_EQ(ref, *output);
    EXPECT_EQ(1, decoder.getTypeRefSize());
  }

  {
    std::string data{0x53, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o'};
    unsigned char buf[] = {0x90};
    // Append type ref number 1
    std::string data2(reinterpret_cast<char *>(buf), 1);
    data.append(data2);

    Hessian2::Decoder decoder(data);
    Object::TypeRef ref("hello");
    auto output = decoder.decode<Object::TypeRef>();
    EXPECT_EQ(ref, *output);
    EXPECT_EQ(1, decoder.getTypeRefSize());
    auto output2 = decoder.decode<Object::TypeRef>();
    EXPECT_EQ(ref, *output2);
    EXPECT_EQ(1, decoder.getTypeRefSize());
  }
}

TEST(TypeRefCodecTest, encode) {
  {
    std::string expect_data{0x05, 'h', 'e', 'l', 'l', 'o'};
    unsigned char buf[] = {0x90};
    // Append type ref number 1
    std::string data2(reinterpret_cast<char *>(buf), 1);
    expect_data.append(data2);

    std::string output;
    Hessian2::Encoder encoder(output);
    Object::TypeRef ref("hello");
    encoder.encode<Object::TypeRef>(ref);
    encoder.encode<Object::TypeRef>(ref);
    EXPECT_EQ(expect_data, output);
    EXPECT_EQ(1, encoder.getTypeRefSize());
  }
}

}  // namespace Hessian2
