#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/object.hpp"
#include "hessian2/basic_codec/object_codec.hpp"

#include "hessian2/basic_codec/class_instance_codec.hpp"
#include "hessian2/basic_codec/list_codec.hpp"
#include "hessian2/basic_codec/ref_object_codec.hpp"
#include "hessian2/test_framework/decoder_test_framework.h"
#include "hessian2/test_framework/encoder_test_framework.h"

namespace Hessian2 {

class ClassInstanceTest : public testing::Test {
 public:
  void decodeSucc(absl::string_view data, ClassInstanceObject out) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<ClassInstanceObject>();
    EXPECT_EQ(out, *output);
    EXPECT_EQ(1, decoder.offset());
  }

  void decodeFail(absl::string_view data) {
    Hessian2::Decoder decoder(data);
    auto output = decoder.decode<ClassInstanceObject>();
    EXPECT_EQ(nullptr, output);
  }

  void encodeSucc(ClassInstanceObject data, std::string expected_data = "") {
    std::string res;
    Hessian2::Encoder encoder(res);
    encoder.encode<ClassInstanceObject>(data);
    if (!expected_data.empty()) {
      EXPECT_EQ(expected_data, res);
    }
    // decodeSucc(res, std::move(data));
  }
};

TEST_F(ClassInstanceTest, InsufficientDataDecode) {
  {
    std::string data;
    decodeFail(data);
  }

  {
    std::string data{'C'};
    decodeFail(data);
  }

  {
    std::string data{'C', 'D'};
    decodeFail(data);
  }

  {
    std::string data{'D'};
    decodeFail(data);
  }
}

TEST_F(TestDecoderFramework, DecoderJavaTestCaseForClassInstance) {
  {
    Object::ClassInstance o;
    o.def_ = std::make_shared<Object::RawDefinition>();
    o.def_->type_ = "com.caucho.hessian.test.A0";
    ClassInstanceObject object(std::move(o));
    EXPECT_TRUE(Decode<ClassInstanceObject>("replyObject_0", object));
  }

  {
    Object::ClassInstance o;
    o.def_ = std::make_shared<Object::RawDefinition>();
    o.def_->type_ = "com.caucho.hessian.test.TestObject";
    o.def_->field_names_.emplace_back("_value");
    o.data_.push_back(std::make_unique<IntegerObject>(0));
    ClassInstanceObject object(std::move(o));
    EXPECT_TRUE(Decode<ClassInstanceObject>("replyObject_1", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.TestObject";
    o1.def_->field_names_.emplace_back("_value");
    o1.data_.push_back(std::make_unique<IntegerObject>(0));
    Object::ClassInstance o2;
    o2.def_ = std::make_shared<Object::RawDefinition>();
    o2.def_->type_ = "com.caucho.hessian.test.TestObject";
    o2.def_->field_names_.emplace_back("_value");
    o2.data_.push_back(std::make_unique<IntegerObject>(1));

    Object::UntypedList o;
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o1)));
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o2)));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<UntypedListObject>("replyObject_2", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.TestObject";
    o1.def_->field_names_.emplace_back("_value");
    o1.data_.push_back(std::make_unique<IntegerObject>(0));

    auto o1_obj = std::make_unique<ClassInstanceObject>(std::move(o1));
    auto o2_obj = std::make_unique<RefObject>(o1_obj.get());

    Object::UntypedList o;
    o.emplace_back(std::move(o1_obj));
    o.emplace_back(std::move(o2_obj));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<UntypedListObject>("replyObject_2a", object, true));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.TestObject";
    o1.def_->field_names_.emplace_back("_value");
    o1.data_.push_back(std::make_unique<IntegerObject>(0));

    Object::ClassInstance o2;
    o2.def_ = std::make_shared<Object::RawDefinition>();
    o2.def_->type_ = "com.caucho.hessian.test.TestObject";
    o2.def_->field_names_.emplace_back("_value");
    o2.data_.push_back(std::make_unique<IntegerObject>(0));

    Object::UntypedList o;
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o1)));
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o2)));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<UntypedListObject>("replyObject_2b", object));
  }

  {
    ClassInstanceObject object;
    Object::ClassInstance o;
    o.def_ = std::make_shared<Object::RawDefinition>();
    o.def_->type_ = "com.caucho.hessian.test.TestCons";
    o.def_->field_names_.emplace_back("_first");
    o.def_->field_names_.emplace_back("_rest");
    o.data_.push_back(std::make_unique<StringObject>(absl::string_view("a")));
    auto o2_obj = std::make_unique<RefObject>(&object);
    o.data_.push_back(std::move(o2_obj));
    object.setClassInstance(std::move(o));
    EXPECT_TRUE(Decode<ClassInstanceObject>("replyObject_3", object, true));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.A0";

    Object::UntypedList o;
    for (int i = 0; i <= 16; i++) {
      Object::ClassInstance o1;
      o1.def_ = std::make_shared<Object::RawDefinition>();
      o1.def_->type_ = absl::StrFormat("com.caucho.hessian.test.A%d", i);
      o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o1)));
    }

    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Decode<UntypedListObject>("replyObject_16", object));
  }
}

TEST_F(TestEncoderFramework, EncoderJavaTestCaseForClassInstance) {
  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "java.math.BigInteger";
    o1.def_->field_names_.emplace_back("mag");
    o1.def_->field_names_.emplace_back("firstNonzeroIntNumPlusTwo");
    o1.def_->field_names_.emplace_back("lowestSetBitPlusTwo");
    o1.def_->field_names_.emplace_back("bitLengthPlusOne");
    o1.def_->field_names_.emplace_back("bitCountPlusOne");
    o1.def_->field_names_.emplace_back("signum");

    Object::TypedList o2;
    o2.type_name_ = "[int";
    o2.values_.emplace_back(std::make_unique<IntegerObject>(1));
    o2.values_.emplace_back(std::make_unique<IntegerObject>(2));
    o1.data_.emplace_back(std::make_unique<TypedListObject>(std::move(o2)));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(1));

    ClassInstanceObject object(std::move(o1));
    EXPECT_TRUE(
        Encode<ClassInstanceObject>("customArgTypedFixed_Integer", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "java.math.BigInteger";
    o1.def_->field_names_.emplace_back("mag");
    o1.def_->field_names_.emplace_back("firstNonzeroIntNumPlusTwo");
    o1.def_->field_names_.emplace_back("lowestSetBitPlusTwo");
    o1.def_->field_names_.emplace_back("bitLengthPlusOne");
    o1.def_->field_names_.emplace_back("bitCountPlusOne");
    o1.def_->field_names_.emplace_back("signum");

    Object::TypedList o2;
    o2.type_name_ = "[int";
    o2.values_.emplace_back(std::make_unique<IntegerObject>(1));
    o2.values_.emplace_back(std::make_unique<IntegerObject>(2));
    o1.data_.emplace_back(std::make_unique<TypedListObject>(std::move(o2)));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(0));
    o1.data_.emplace_back(std::make_unique<IntegerObject>(-2));

    ClassInstanceObject object(std::move(o1));
    EXPECT_TRUE(Encode<ClassInstanceObject>("customArgTypedFixed_IntegerSigned",
                                            object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "java.math.BigDecimal";
    o1.def_->field_names_.emplace_back("value");
    o1.data_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("100.256")));

    ClassInstanceObject object(std::move(o1));
    EXPECT_TRUE(
        Encode<ClassInstanceObject>("customArgTypedFixed_Decimal", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "test.Dog";
    o1.def_->field_names_.emplace_back("name");
    o1.def_->field_names_.emplace_back("gender");
    o1.data_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("a dog")));
    o1.data_.emplace_back(
        std::make_unique<StringObject>(absl::string_view("male")));

    ClassInstanceObject object(std::move(o1));
    EXPECT_TRUE(
        Encode<ClassInstanceObject>("customArgTypedFixed_Extends", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "test.model.DateDemo";
    o1.def_->field_names_.emplace_back("name");
    o1.def_->field_names_.emplace_back("date");
    o1.def_->field_names_.emplace_back("date1");
    o1.data_.emplace_back(std::make_unique<NullObject>());
    o1.data_.emplace_back(std::make_unique<NullObject>());
    o1.data_.emplace_back(std::make_unique<NullObject>());

    ClassInstanceObject object(std::move(o1));
    EXPECT_TRUE(
        Encode<ClassInstanceObject>("customArgTypedFixed_DateNull", object));
  }

  {
    Object::ClassInstance o;
    o.def_ = std::make_shared<Object::RawDefinition>();
    o.def_->type_ = "com.caucho.hessian.test.A0";
    ClassInstanceObject object(std::move(o));
    EXPECT_TRUE(Encode<ClassInstanceObject>("argObject_0", object));
  }

  {
    Object::ClassInstance o;
    o.def_ = std::make_shared<Object::RawDefinition>();
    o.def_->type_ = "com.caucho.hessian.test.TestObject";
    o.def_->field_names_.emplace_back("_value");
    o.data_.push_back(std::make_unique<IntegerObject>(0));
    ClassInstanceObject object(std::move(o));
    EXPECT_TRUE(Encode<ClassInstanceObject>("argObject_1", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.TestObject";
    o1.def_->field_names_.emplace_back("_value");
    o1.data_.push_back(std::make_unique<IntegerObject>(0));
    Object::ClassInstance o2;
    o2.def_ = std::make_shared<Object::RawDefinition>();
    o2.def_->type_ = "com.caucho.hessian.test.TestObject";
    o2.def_->field_names_.emplace_back("_value");
    o2.data_.push_back(std::make_unique<IntegerObject>(1));

    Object::UntypedList o;
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o1)));
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o2)));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<UntypedListObject>("argObject_2", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.TestObject";
    o1.def_->field_names_.emplace_back("_value");
    o1.data_.push_back(std::make_unique<IntegerObject>(0));

    auto o1_obj = std::make_unique<ClassInstanceObject>(std::move(o1));
    auto o2_obj = std::make_unique<RefObject>(o1_obj.get());

    Object::UntypedList o;
    o.emplace_back(std::move(o1_obj));
    o.emplace_back(std::move(o2_obj));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<UntypedListObject>("argObject_2a", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.TestObject";
    o1.def_->field_names_.emplace_back("_value");
    o1.data_.push_back(std::make_unique<IntegerObject>(0));

    Object::ClassInstance o2;
    o2.def_ = std::make_shared<Object::RawDefinition>();
    o2.def_->type_ = "com.caucho.hessian.test.TestObject";
    o2.def_->field_names_.emplace_back("_value");
    o2.data_.push_back(std::make_unique<IntegerObject>(0));

    Object::UntypedList o;
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o1)));
    o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o2)));
    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<UntypedListObject>("argObject_2b", object));
  }

  {
    ClassInstanceObject object;
    Object::ClassInstance o;
    o.def_ = std::make_shared<Object::RawDefinition>();
    o.def_->type_ = "com.caucho.hessian.test.TestCons";
    o.def_->field_names_.emplace_back("_first");
    o.def_->field_names_.emplace_back("_rest");
    o.data_.push_back(std::make_unique<StringObject>(absl::string_view("a")));
    auto o2_obj = std::make_unique<RefObject>(&object);
    o.data_.push_back(std::move(o2_obj));
    object.setClassInstance(std::move(o));
    EXPECT_TRUE(Encode<ClassInstanceObject>("argObject_3", object));
  }

  {
    Object::ClassInstance o1;
    o1.def_ = std::make_shared<Object::RawDefinition>();
    o1.def_->type_ = "com.caucho.hessian.test.A0";

    Object::UntypedList o;
    for (int i = 0; i <= 16; i++) {
      Object::ClassInstance o1;
      o1.def_ = std::make_shared<Object::RawDefinition>();
      o1.def_->type_ = absl::StrFormat("com.caucho.hessian.test.A%d", i);
      o.emplace_back(std::make_unique<ClassInstanceObject>(std::move(o1)));
    }

    UntypedListObject object(std::move(o));
    EXPECT_TRUE(Encode<UntypedListObject>("argObject_16", object));
  }
}

}  // namespace Hessian2
