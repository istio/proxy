#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/object_codec.hpp"
#include "hessian2/object.hpp"

namespace Hessian2 {

class ObjectCodecTest : public testing::Test {
 public:
  template <typename T>
  void decodeSucc(absl::string_view data, const T &out, size_t size) {
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
  void encodeSucc(const T &data, size_t size, std::string expected_data = "") {
    std::string res;
    Hessian2::Encoder encoder(res);
    encoder.encode<T>(data);
    if (!expected_data.empty()) {
      EXPECT_EQ(expected_data, res);
    }
    decodeSucc<T>(res, data, size);
  }
};

TEST_F(ObjectCodecTest, Decode) {
  {
    std::string data;
    decodeFail<Object>(data);
  }
  {
    // Null Object
    std::string data{'N'};
    NullObject o;
    decodeSucc<Object>(data, o, 1);
  }
  {
    // String Object
    std::string data{0x53, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o'};
    StringObject o(absl::string_view("hello"));
    decodeSucc<Object>(data, o, 8);
  }
  {
    // Binary Object
    std::string data{0x23, 0x01, 0x02, 0x03};
    BinaryObject o(std::vector<uint8_t>{1, 2, 3});
    decodeSucc<Object>(data, o, 4);
  }

  {
    // Boolean Object
    std::string data{'T'};
    BooleanObject o(true);
    decodeSucc<Object>(data, o, 1);
  }
  {
    // Boolean Object
    std::string data{'F'};
    BooleanObject o(false);
    decodeSucc<Object>(data, o, 1);
  }

  {
    // Int Object
    unsigned char buf[] = {0x49, 0x00, 0x00, 0x01, 0x2c};
    std::string data(reinterpret_cast<char *>(buf), 5);
    IntegerObject o(300);
    decodeSucc<Object>(data, o, 5);
  }

  {
    // Long Object
    unsigned char buf[] = {0x59, 0x00, 0x00, 0x01, 0x2c};
    std::string data(reinterpret_cast<char *>(buf), 5);
    LongObject o(300);
    decodeSucc<Object>(data, o, 5);
  }

  // eight octet longs
  {
    unsigned char buf[] = {0x4c, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x01, 0x2c};
    std::string data(reinterpret_cast<char *>(buf), 9);
    LongObject o(300);
    decodeSucc<Object>(data, o, 9);
  }

  {
    // Date Object
    unsigned char buf[] = {0x4b, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 5);
    DateObject o(std::chrono::milliseconds(0));
    decodeSucc<Object>(data, o, 5);
  }

  {
    // Double Object
    unsigned char buf[] = {0x5d, 0x7f};
    std::string data(reinterpret_cast<char *>(buf), 2);
    DoubleObject o(127.0);
    decodeSucc<Object>(data, o, 2);
  }

  {
    // Typed list Object
    unsigned char buf[] = {'V', 0x04, '[', 'i', 'n', 't', 0x92, 0x90, 0x91};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<IntegerObject>(0));
    o_list.emplace_back(std::make_unique<IntegerObject>(1));
    std::string type = "[int";
    TypedListObject o(std::move(type), std::move(o_list));
    decodeSucc<Object>(data, o, sizeof(buf));
  }

  {
    // Untyped list Object
    unsigned char buf[] = {0x57, 0x90, 0x91, 'Z'};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<IntegerObject>(0));
    o_list.emplace_back(std::make_unique<IntegerObject>(1));
    UntypedListObject o(std::move(o_list));
    decodeSucc<Object>(data, o, sizeof(buf));
  }

  {
    // Typed map Object
    unsigned char buf[] = {'M', 0x03, 'c',  'o', 'm', 0x05, 'c', 'o', 'l',
                           'o', 'r',  0x05, 'h', 'e', 'l',  'l', 'o', 'Z'};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::TypedMap o_map;
    o_map.type_name_ = "com";
    o_map.field_name_and_value_.emplace(
        std::make_unique<StringObject>(absl::string_view("color")),
        std::make_unique<StringObject>(absl::string_view("hello")));
    TypedMapObject o(std::move(o_map));
    decodeSucc<Object>(data, o, sizeof(buf));
  }

  {
    // Untyped map Object
    unsigned char buf[] = {'H',  0x05, 'c', 'o', 'l', 'o', 'r',
                           0x05, 'h',  'e', 'l', 'l', 'o', 'Z'};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::UntypedMap o_map;
    o_map.emplace(std::make_unique<StringObject>(absl::string_view("color")),
                  std::make_unique<StringObject>(absl::string_view("hello")));
    UntypedMapObject o(std::move(o_map));
    decodeSucc<Object>(data, o, sizeof(buf));
  }

  {
    // Class instance Object
    unsigned char buf[] = {'C',  0x05, 'h',  'e',  'l', 'l',  'o', 0x92, 0x05,
                           'c',  'o',  'l',  'o',  'r', 0x05, 'm', 'o',  'd',
                           'e',  'l',  0x60, 0x05, 'g', 'r',  'e', 'e',  'n',
                           0x05, 'c',  'i',  'v',  'i', 'c'};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::ClassInstance instance;
    instance.data_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("green")));
    instance.data_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("civic")));
    instance.def_ = std::make_shared<Object::RawDefinition>(
        "hello", std::vector<std::string>{"color", "model"});
    ClassInstanceObject o(std::move(instance));
    decodeSucc<Object>(data, o, sizeof(buf));
  }
}

TEST_F(ObjectCodecTest, encode) {
  {
    // String Object
    std::string data{0x05, 'h', 'e', 'l', 'l', 'o'};
    StringObject o(absl::string_view("hello"));
    encodeSucc<Object>(o, data.size(), data);
  }
  {
    // Binary Object
    std::string data{0x23, 0x01, 0x02, 0x03};
    BinaryObject o(std::vector<uint8_t>{1, 2, 3});
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Boolean Object
    std::string data{'F'};
    BooleanObject o(false);
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Int Object
    std::string data{'\xcf', '\xff'};
    IntegerObject o(2047);
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Long Object
    std::string data{'\xff', '\xff'};
    LongObject o(2047);
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Date Object
    unsigned char buf[] = {0x4b, 0x00, 0x00, 0x00, 0x00};
    std::string data(reinterpret_cast<char *>(buf), 5);
    DateObject o(std::chrono::milliseconds(0));
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Double Object
    unsigned char buf[] = {0x5d, 0x7f};
    std::string data(reinterpret_cast<char *>(buf), 2);
    DoubleObject o(127.0);
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Typed list Object
    unsigned char buf[] = {0x72, 0x04, '[', 'i', 'n', 't', 0x90, 0x91};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<IntegerObject>(0));
    o_list.emplace_back(std::make_unique<IntegerObject>(1));
    std::string type = "[int";
    TypedListObject o(std::move(type), std::move(o_list));
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Untyped list Object
    unsigned char buf[] = {0x7a, 0x90, 0x91};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<IntegerObject>(0));
    o_list.emplace_back(std::make_unique<IntegerObject>(1));
    UntypedListObject o(std::move(o_list));
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Typed map Object
    unsigned char buf[] = {'M', 0x03, 'c',  'o', 'm', 0x05, 'c', 'o', 'l',
                           'o', 'r',  0x05, 'h', 'e', 'l',  'l', 'o', 'Z'};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::TypedMap o_map;
    o_map.type_name_ = "com";
    o_map.field_name_and_value_.emplace(
        std::make_unique<StringObject>(absl::string_view("color")),
        std::make_unique<StringObject>(absl::string_view("hello")));
    TypedMapObject o(std::move(o_map));
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Untyped map Object
    unsigned char buf[] = {'H',  0x05, 'c', 'o', 'l', 'o', 'r',
                           0x05, 'h',  'e', 'l', 'l', 'o', 'Z'};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::UntypedMap o_map;
    o_map.emplace(std::make_unique<StringObject>(absl::string_view("color")),
                  std::make_unique<StringObject>(absl::string_view("hello")));
    UntypedMapObject o(std::move(o_map));
    encodeSucc<Object>(o, data.size(), data);
  }

  {
    // Class instance Object
    unsigned char buf[] = {'C',  0x05, 'h',  'e',  'l', 'l',  'o', 0x92, 0x05,
                           'c',  'o',  'l',  'o',  'r', 0x05, 'm', 'o',  'd',
                           'e',  'l',  0x60, 0x05, 'g', 'r',  'e', 'e',  'n',
                           0x05, 'c',  'i',  'v',  'i', 'c'};
    std::string data(reinterpret_cast<char *>(buf), sizeof(buf));
    Object::ClassInstance instance;
    instance.data_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("green")));
    instance.data_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("civic")));
    instance.def_ = std::make_shared<Object::RawDefinition>(
        "hello", std::vector<std::string>{"color", "model"});
    ClassInstanceObject o(std::move(instance));
    encodeSucc<Object>(o, data.size(), data);
  }
}

}  // namespace Hessian2
