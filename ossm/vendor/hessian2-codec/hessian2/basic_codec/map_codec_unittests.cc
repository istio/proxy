#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/object_codec.hpp"
#include "hessian2/basic_codec/list_codec.hpp"
#include "hessian2/basic_codec/map_codec.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace Hessian2 {

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForMap) {
  {
    Object::UntypedMap o;
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<UntypedMapObject>("replyUntypedMap_0", obj));
  }

  {
    Object::UntypedMap o;
    o.emplace(std::make_unique<StringObject>(absl::string_view("a")),
              std::make_unique<IntegerObject>(0));
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<UntypedMapObject>("replyUntypedMap_1", obj));
  }

  {
    Object::UntypedMap o;
    o.emplace(std::make_unique<IntegerObject>(0),
              std::make_unique<StringObject>(absl::string_view("a")));
    o.emplace(std::make_unique<IntegerObject>(1),
              std::make_unique<StringObject>(absl::string_view("b")));
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<UntypedMapObject>("replyUntypedMap_2", obj));
  }

  {
    Object::UntypedMap o;
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<StringObject>(absl::string_view("a")));
    o.emplace(std::make_unique<UntypedListObject>(std::move(o_list)),
              std::make_unique<IntegerObject>(0));
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<UntypedMapObject>("replyUntypedMap_3", obj));
  }

  {
    Object::TypedMap o;
    o.type_name_ = "java.util.Hashtable";
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<TypedMapObject>("replyTypedMap_0", obj));
  }

  {
    Object::TypedMap o;
    o.type_name_ = "java.util.Hashtable";
    o.field_name_and_value_.emplace(
        std::make_unique<StringObject>(absl::string_view("a")),
        std::make_unique<IntegerObject>(0));
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<TypedMapObject>("replyTypedMap_1", obj));
  }

  {
    Object::TypedMap o;
    o.type_name_ = "java.util.Hashtable";
    o.field_name_and_value_.emplace(
        std::make_unique<IntegerObject>(0),
        std::make_unique<StringObject>(absl::string_view("a")));
    o.field_name_and_value_.emplace(
        std::make_unique<IntegerObject>(1),
        std::make_unique<StringObject>(absl::string_view("b")));
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<TypedMapObject>("replyTypedMap_2", obj));
  }

  {
    Object::TypedMap o;
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<StringObject>(absl::string_view("a")));
    o.type_name_ = "java.util.Hashtable";
    o.field_name_and_value_.emplace(
        std::make_unique<UntypedListObject>(std::move(o_list)),
        std::make_unique<IntegerObject>(0));
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Decode<TypedMapObject>("replyTypedMap_3", obj));
  }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForMap) {
  {
    Object::UntypedMap o;
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<UntypedMapObject>("argUntypedMap_0", obj));
  }

  {
    Object::UntypedMap o;
    o.emplace(std::make_unique<StringObject>(absl::string_view("a")),
              std::make_unique<IntegerObject>(0));
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<UntypedMapObject>("argUntypedMap_1", obj));
  }

  {
    Object::UntypedMap o;
    o.emplace(std::make_unique<IntegerObject>(0),
              std::make_unique<StringObject>(absl::string_view("a")));
    o.emplace(std::make_unique<IntegerObject>(1),
              std::make_unique<StringObject>(absl::string_view("b")));
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<UntypedMapObject>("argUntypedMap_2", obj));
  }

  {
    Object::UntypedMap o;
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<StringObject>(absl::string_view("a")));
    o.emplace(std::make_unique<UntypedListObject>(std::move(o_list)),
              std::make_unique<IntegerObject>(0));
    UntypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<UntypedMapObject>("argUntypedMap_3", obj));
  }

  {
    Object::TypedMap o;
    o.type_name_ = "java.util.Hashtable";
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<TypedMapObject>("argTypedMap_0", obj));
  }

  {
    Object::TypedMap o;
    o.type_name_ = "java.util.Hashtable";
    o.field_name_and_value_.emplace(
        std::make_unique<StringObject>(absl::string_view("a")),
        std::make_unique<IntegerObject>(0));
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<TypedMapObject>("argTypedMap_1", obj));
  }

  {
    Object::TypedMap o;
    o.type_name_ = "java.util.Hashtable";
    o.field_name_and_value_.emplace(
        std::make_unique<IntegerObject>(0),
        std::make_unique<StringObject>(absl::string_view("a")));
    o.field_name_and_value_.emplace(
        std::make_unique<IntegerObject>(1),
        std::make_unique<StringObject>(absl::string_view("b")));
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<TypedMapObject>("argTypedMap_2", obj));
  }

  {
    Object::TypedMap o;
    Object::UntypedList o_list;
    o_list.emplace_back(std::make_unique<StringObject>(absl::string_view("a")));
    o.type_name_ = "java.util.Hashtable";
    o.field_name_and_value_.emplace(
        std::make_unique<UntypedListObject>(std::move(o_list)),
        std::make_unique<IntegerObject>(0));
    TypedMapObject obj(std::move(o));
    EXPECT_TRUE(Encode<TypedMapObject>("argTypedMap_3", obj));
  }
}

}  // namespace Hessian2
