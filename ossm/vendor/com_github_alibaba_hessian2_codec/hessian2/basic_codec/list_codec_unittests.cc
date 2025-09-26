#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/basic_codec/object_codec.hpp"
#include "hessian2/basic_codec/list_codec.hpp"
#include "hessian2/basic_codec/class_instance_codec.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace Hessian2 {

class ListCodecTest : public testing::Test {
 public:
  void decodeTypeListFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<TypedListObject>();
    EXPECT_EQ(nullptr, output);
  }

  void decodeUntypedListFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<UntypedListObject>();
    EXPECT_EQ(nullptr, output);
  }
};

// Insufficient Data
TEST_F(ListCodecTest, InsufficientData) {
  {
    std::string data;
    decodeTypeListFail(data);
    decodeUntypedListFail(data);
  }

  {
    std::string data{0x55, 0x00};
    decodeTypeListFail(data);
  }

  {
    std::string data{0x55, 0x20, 0x08};
    decodeTypeListFail(data);
  }
}

std::unique_ptr<ClassInstanceObject> GenerateTypedListTestObject() {
  Object::ClassInstance o;
  o.def_ = std::make_shared<Object::RawDefinition>();
  o.def_->type_ = "test.TypedListTest";
  o.def_->field_names_.emplace_back("a");
  o.def_->field_names_.emplace_back("list");
  o.def_->field_names_.emplace_back("list1");

  std::vector<Object::ClassInstance> cls;

  for (int i = 0; i < 5; i++) {
    Object::ClassInstance c;
    c.def_ = std::make_shared<Object::RawDefinition>();
    c.def_->type_ = "com.caucho.hessian.test.A0";
    cls.emplace_back(std::move(c));
  }

  for (int i = 0; i < 4; i++) {
    Object::ClassInstance c;
    c.def_ = std::make_shared<Object::RawDefinition>();
    c.def_->type_ = "com.caucho.hessian.test.A1";
    cls.emplace_back(std::move(c));
  }
  {
    Object::TypedList o1;
    o1.type_name_ = "[com.caucho.hessian.test.A0";
    o1.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[0])));
    o1.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[1])));
    auto object1 = std::make_unique<TypedListObject>(std::move(o1));

    Object::TypedList o2;
    o2.type_name_ = "[com.caucho.hessian.test.A0";
    o2.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[2])));
    o2.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[3])));
    auto object2 = std::make_unique<TypedListObject>(std::move(o2));

    o.data_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[4])));

    Object::TypedList o3;
    o3.type_name_ = "[[com.caucho.hessian.test.A0";
    o3.values_.emplace_back(std::move(object1));
    o3.values_.emplace_back(std::move(object2));
    auto object3 = std::make_unique<TypedListObject>(std::move(o3));
    o.data_.emplace_back(std::move(object3));
  }

  {
    Object::TypedList o1;
    o1.type_name_ = "[com.caucho.hessian.test.A1";
    o1.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[5])));
    o1.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[6])));
    auto object1 = std::make_unique<TypedListObject>(std::move(o1));

    Object::TypedList o2;
    o2.type_name_ = "[com.caucho.hessian.test.A1";
    o2.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[7])));
    o2.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[8])));
    auto object2 = std::make_unique<TypedListObject>(std::move(o2));

    Object::TypedList o3;
    o3.type_name_ = "[[com.caucho.hessian.test.A1";
    o3.values_.emplace_back(std::move(object1));
    o3.values_.emplace_back(std::move(object2));
    auto object3 = std::make_unique<TypedListObject>(std::move(o3));
    o.data_.emplace_back(std::move(object3));
  }
  auto class_instance = std::make_unique<ClassInstanceObject>(std::move(o));
  return class_instance;
}

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForList) {
  {
    UntypedListObject object;
    EXPECT_TRUE(Decode<UntypedListObject>("replyUntypedFixedList_0", object));
  }

  {
    Object::UntypedList o;
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("1")));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<UntypedListObject>("replyUntypedFixedList_1", object));
  }

  {
    Object::UntypedList o;
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("1")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("2")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("3")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("4")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("5")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("6")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("7")));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<UntypedListObject>("replyUntypedFixedList_7", object));
  }

  {
    Object::UntypedList o;
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("1")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("2")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("3")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("4")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("5")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("6")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("7")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("8")));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<UntypedListObject>("replyUntypedFixedList_8", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<TypedListObject>("replyTypedFixedList_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("1")));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<TypedListObject>("replyTypedFixedList_1", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("1")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("2")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("3")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("4")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("5")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("6")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("7")));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<TypedListObject>("replyTypedFixedList_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("1")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("2")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("3")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("4")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("5")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("6")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("7")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("8")));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<TypedListObject>("replyTypedFixedList_8", object));
  }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForList) {
  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.A0";

    Object::ClassInstance o2;
    o2.def_ = std::make_shared<Object::RawDefinition>();
    o2.def_->type_ = "com.caucho.hessian.test.A1";

    Object::UntypedList o;
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o1)));
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o2)));
    o.emplace_back(std::make_unique<NullObject>());
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<UntypedListObject>("customArgUntypedFixedListHasNull", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.A0";
    Object::TypedList o;
    o.type_name_ = "[com.caucho.hessian.test.A0";
    o.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(o1)));

    TypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<TypedListObject>("customArgTypedFixedList", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[short";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_short_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[short";
    o.values_.emplace_back(std::make_unique<IntegerObject>(1));
    o.values_.emplace_back(std::make_unique<IntegerObject>(2));
    o.values_.emplace_back(std::make_unique<IntegerObject>(3));
    o.values_.emplace_back(std::make_unique<IntegerObject>(4));
    o.values_.emplace_back(std::make_unique<IntegerObject>(5));
    o.values_.emplace_back(std::make_unique<IntegerObject>(6));
    o.values_.emplace_back(std::make_unique<IntegerObject>(7));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_short_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[int";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_int_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[int";
    o.values_.emplace_back(std::make_unique<IntegerObject>(1));
    o.values_.emplace_back(std::make_unique<IntegerObject>(2));
    o.values_.emplace_back(std::make_unique<IntegerObject>(3));
    o.values_.emplace_back(std::make_unique<IntegerObject>(4));
    o.values_.emplace_back(std::make_unique<IntegerObject>(5));
    o.values_.emplace_back(std::make_unique<IntegerObject>(6));
    o.values_.emplace_back(std::make_unique<IntegerObject>(7));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_int_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[long";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_long_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[long";
    o.values_.emplace_back(std::make_unique<LongObject>(1));
    o.values_.emplace_back(std::make_unique<LongObject>(2));
    o.values_.emplace_back(std::make_unique<LongObject>(3));
    o.values_.emplace_back(std::make_unique<LongObject>(4));
    o.values_.emplace_back(std::make_unique<LongObject>(5));
    o.values_.emplace_back(std::make_unique<LongObject>(6));
    o.values_.emplace_back(std::make_unique<LongObject>(7));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_long_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[float";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_float_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[float";
    o.values_.emplace_back(std::make_unique<DoubleObject>(1));
    o.values_.emplace_back(std::make_unique<DoubleObject>(2));
    o.values_.emplace_back(std::make_unique<DoubleObject>(3));
    o.values_.emplace_back(std::make_unique<DoubleObject>(4));
    o.values_.emplace_back(std::make_unique<DoubleObject>(5));
    o.values_.emplace_back(std::make_unique<DoubleObject>(6));
    o.values_.emplace_back(std::make_unique<DoubleObject>(7));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_float_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[double";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_double_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[double";
    o.values_.emplace_back(std::make_unique<DoubleObject>(1));
    o.values_.emplace_back(std::make_unique<DoubleObject>(2));
    o.values_.emplace_back(std::make_unique<DoubleObject>(3));
    o.values_.emplace_back(std::make_unique<DoubleObject>(4));
    o.values_.emplace_back(std::make_unique<DoubleObject>(5));
    o.values_.emplace_back(std::make_unique<DoubleObject>(6));
    o.values_.emplace_back(std::make_unique<DoubleObject>(7));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_double_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[boolean";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_boolean_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[boolean";
    o.values_.emplace_back(std::make_unique<BooleanObject>(true));
    o.values_.emplace_back(std::make_unique<BooleanObject>(false));
    o.values_.emplace_back(std::make_unique<BooleanObject>(true));
    o.values_.emplace_back(std::make_unique<BooleanObject>(false));
    o.values_.emplace_back(std::make_unique<BooleanObject>(true));
    o.values_.emplace_back(std::make_unique<BooleanObject>(false));
    o.values_.emplace_back(std::make_unique<BooleanObject>(true));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_boolean_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[java.util.Date";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_date_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[java.util.Date";
    o.values_.emplace_back(
        std::make_unique<DateObject>(std::chrono::milliseconds(1560864000)));
    o.values_.emplace_back(
        std::make_unique<DateObject>(std::chrono::milliseconds(1560864000)));
    o.values_.emplace_back(
        std::make_unique<DateObject>(std::chrono::milliseconds(1560864000)));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_date_3", object));
  }

  {
    Object::TypedList o1;
    o1.type_name_ = "[int";
    o1.values_.emplace_back(std::make_unique<IntegerObject>(1));
    o1.values_.emplace_back(std::make_unique<IntegerObject>(2));
    o1.values_.emplace_back(std::make_unique<IntegerObject>(3));
    auto object1 = std::make_unique<TypedListObject>(std::move(o1));

    Object::TypedList o2;
    o2.type_name_ = "[int";
    o2.values_.emplace_back(std::make_unique<IntegerObject>(4));
    o2.values_.emplace_back(std::make_unique<IntegerObject>(5));
    o2.values_.emplace_back(std::make_unique<IntegerObject>(6));
    o2.values_.emplace_back(std::make_unique<IntegerObject>(7));
    auto object2 = std::make_unique<TypedListObject>(std::move(o2));

    Object::TypedList o3;
    o3.type_name_ = "[int";
    o3.values_.emplace_back(std::make_unique<IntegerObject>(8));
    o3.values_.emplace_back(std::make_unique<IntegerObject>(9));
    o3.values_.emplace_back(std::make_unique<IntegerObject>(10));
    auto object3 = std::make_unique<TypedListObject>(std::move(o3));

    Object::TypedList o4;
    o4.type_name_ = "[int";
    o4.values_.emplace_back(std::make_unique<IntegerObject>(11));
    o4.values_.emplace_back(std::make_unique<IntegerObject>(12));
    o4.values_.emplace_back(std::make_unique<IntegerObject>(13));
    o4.values_.emplace_back(std::make_unique<IntegerObject>(14));
    auto object4 = std::make_unique<TypedListObject>(std::move(o4));

    Object::TypedList o5;
    o5.type_name_ = "[[int";
    o5.values_.emplace_back(std::move(object1));
    o5.values_.emplace_back(std::move(object2));
    auto object5 = std::make_unique<TypedListObject>(std::move(o5));

    Object::TypedList o6;
    o6.type_name_ = "[[int";
    o6.values_.emplace_back(std::move(object3));
    o6.values_.emplace_back(std::move(object4));
    auto object6 = std::make_unique<TypedListObject>(std::move(o6));

    Object::TypedList o7;
    o7.type_name_ = "[[[int";
    o7.values_.emplace_back(std::move(object5));
    o7.values_.emplace_back(std::move(object6));
    auto object7 = std::make_unique<TypedListObject>(std::move(o7));

    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_arrays", *object7));
  }

  {
    std::vector<Object::ClassInstance> cls;

    for (int i = 0; i < 8; i++) {
      Object::ClassInstance c;
      c.def_ = std::make_shared<Object::RawDefinition>();
      c.def_->type_ = "com.caucho.hessian.test.A0";
      cls.emplace_back(std::move(c));
    }

    Object::TypedList o1;
    o1.type_name_ = "[com.caucho.hessian.test.A0";
    o1.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[0])));
    o1.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[1])));
    o1.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[2])));
    auto object1 = std::make_unique<TypedListObject>(std::move(o1));

    Object::TypedList o2;
    o2.type_name_ = "[com.caucho.hessian.test.A0";
    o2.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[3])));
    o2.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[4])));
    o2.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[5])));
    o2.values_.emplace_back(std::make_unique<NullObject>());
    auto object2 = std::make_unique<TypedListObject>(std::move(o2));

    Object::TypedList o3;
    o3.type_name_ = "[com.caucho.hessian.test.A0";
    o3.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[6])));
    auto object3 = std::make_unique<TypedListObject>(std::move(o3));

    Object::TypedList o4;
    o4.type_name_ = "[com.caucho.hessian.test.A0";
    o4.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(cls[7])));
    auto object4 = std::make_unique<TypedListObject>(std::move(o4));

    Object::TypedList o5;
    o5.type_name_ = "[[com.caucho.hessian.test.A0";
    o5.values_.emplace_back(std::move(object1));
    o5.values_.emplace_back(std::move(object2));
    auto object5 = std::make_unique<TypedListObject>(std::move(o5));

    Object::TypedList o6;
    o6.type_name_ = "[[com.caucho.hessian.test.A0";
    o6.values_.emplace_back(std::move(object3));
    o6.values_.emplace_back(std::move(object4));
    auto object6 = std::make_unique<TypedListObject>(std::move(o6));

    Object::TypedList o7;
    o7.type_name_ = "[[[com.caucho.hessian.test.A0";
    o7.values_.emplace_back(std::move(object5));
    o7.values_.emplace_back(std::move(object6));
    auto object7 = std::make_unique<TypedListObject>(std::move(o7));

    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_A0arrays", *object7));
  }

  {
    std::unique_ptr<ClassInstanceObject> expect_obj =
        GenerateTypedListTestObject();
    EXPECT_TRUE(Encode<ClassInstanceObject>("customArgTypedFixedList_Test",
                                            *expect_obj));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.A0";
    Object::TypedList o;
    o.type_name_ = "[java.lang.Object";
    o.values_.emplace_back(
        std::make_unique<ClassInstanceObject>(std::move(o1)));

    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_Object", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "java.util.HashSet";
    o.values_.emplace_back(std::make_unique<IntegerObject>(0));
    o.values_.emplace_back(std::make_unique<IntegerObject>(1));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(
        Encode<TypedListObject>("customArgTypedFixedList_HashSet", object));
  }

  {
    UntypedListObject object;
    EXPECT_TRUE(Encode<UntypedListObject>("argUntypedFixedList_0", object));
  }

  {
    Object::UntypedList o;
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("1")));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<UntypedListObject>("argUntypedFixedList_1", object));
  }

  {
    Object::UntypedList o;
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("1")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("2")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("3")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("4")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("5")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("6")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("7")));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<UntypedListObject>("argUntypedFixedList_7", object));
  }

  {
    Object::UntypedList o;
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("1")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("2")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("3")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("4")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("5")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("6")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("7")));
    o.emplace_back(std::make_unique<StringObject>(absl::string_view("8")));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<UntypedListObject>("argUntypedFixedList_8", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<TypedListObject>("argTypedFixedList_0", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("1")));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<TypedListObject>("argTypedFixedList_1", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("1")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("2")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("3")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("4")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("5")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("6")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("7")));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<TypedListObject>("argTypedFixedList_7", object));
  }

  {
    Object::TypedList o;
    o.type_name_ = "[string";
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("1")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("2")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("3")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("4")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("5")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("6")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("7")));
    o.values_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("8")));
    TypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<TypedListObject>("argTypedFixedList_8", object));
  }
}

}  // namespace Hessian2
