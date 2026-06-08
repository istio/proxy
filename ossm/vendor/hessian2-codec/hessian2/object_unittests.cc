#include <chrono>
#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "hessian2/object.hpp"

namespace Hessian2 {

// TODO(tianqian.zyf): Refactor the Test case to avoid code duplication
TEST(ObjectTest, BasicOperation) {
  {
    Object::TypeRef r1(absl::string_view("ref1"));
    Object::TypeRef r2(absl::string_view("ref2"));
    Object::TypeRef r3(absl::string_view("ref1"));

    EXPECT_EQ(r1, r3);
    EXPECT_FALSE(r1 == r2);
  }

  {
    Object::RawDefinition r1;
    Object::RawDefinition r2;
    EXPECT_EQ(r1, r2);

    EXPECT_TRUE(r1.toDebugString().size() > 0);
  }
}

TEST(ObjectTest, Binary) {
  std::vector<uint8_t> vec{0x0, 0x1, 0x2, 0x3, 0x4};
  BinaryObject bin(std::vector<uint8_t>{0x0, 0x1, 0x2, 0x3, 0x4});

  EXPECT_NE(bin.toBinary(), absl::nullopt);
  EXPECT_EQ(bin.toInteger(), absl::nullopt);
  EXPECT_EQ(bin.toLong(), absl::nullopt);
  EXPECT_EQ(bin.toDate(), absl::nullopt);
  EXPECT_EQ(bin.toTypedList(), absl::nullopt);
  EXPECT_EQ(bin.toUntypedList(), absl::nullopt);
  EXPECT_EQ(bin.toUntypedMap(), absl::nullopt);
  EXPECT_EQ(bin.toTypedMap(), absl::nullopt);
  EXPECT_EQ(bin.toClassInstance(), absl::nullopt);
  EXPECT_EQ(bin.toRefDest(), absl::nullopt);
  EXPECT_TRUE(bin.toDebugString().size() > 0);

  EXPECT_EQ(bin.type(), Object::Type::Binary);
  EXPECT_TRUE(
      std::equal(vec.begin(), vec.end(), bin.toBinary().value().get().begin()));
  BinaryObject bin2(std::vector<uint8_t>{0x0, 0x1, 0x2, 0x3, 0x4});
  EXPECT_EQ(bin.hash(), bin2.hash());
  EXPECT_TRUE(bin.equal(bin2));
}

TEST(ObjectTest, Boolean) {
  BooleanObject b(true);
  BooleanObject b2(false);
  BooleanObject b3(true);
  EXPECT_EQ(b.type(), Object::Type::Boolean);
  EXPECT_EQ(b.hash(), b3.hash());
  EXPECT_EQ(b.toBoolean().value(), true);
  EXPECT_TRUE(b.hash() != b2.hash());
  EXPECT_TRUE(b.equal(b3));
  EXPECT_FALSE(b.equal(b2));
}

TEST(ObjectTest, Double) {
  DoubleObject b(0.0);
  DoubleObject b2(0.0);
  DoubleObject b3(0.1);

  EXPECT_EQ(b.toBinary(), absl::nullopt);
  EXPECT_EQ(b.toInteger(), absl::nullopt);
  EXPECT_EQ(b.toLong(), absl::nullopt);
  EXPECT_EQ(b.toDate(), absl::nullopt);
  EXPECT_EQ(b.toTypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedMap(), absl::nullopt);
  EXPECT_EQ(b.toTypedMap(), absl::nullopt);
  EXPECT_EQ(b.toClassInstance(), absl::nullopt);
  EXPECT_EQ(b.toRefDest(), absl::nullopt);
  EXPECT_TRUE(b.toDebugString().size() > 0);

  EXPECT_EQ(b.type(), Object::Type::Double);
  EXPECT_EQ(b.hash(), b2.hash());
  EXPECT_EQ(b.toDouble().value(), 0.0);
  EXPECT_TRUE(b.hash() != b3.hash());
  EXPECT_TRUE(b.equal(b2));
  EXPECT_FALSE(b3.equal(b2));
}

TEST(ObjectTest, Integer) {
  IntegerObject b(0);
  IntegerObject b2(0);
  IntegerObject b3(1);
  EXPECT_EQ(b.type(), Object::Type::Integer);
  EXPECT_EQ(b.hash(), b2.hash());
  EXPECT_EQ(b.toInteger().value(), 0);
  EXPECT_TRUE(b.hash() != b3.hash());
  EXPECT_TRUE(b.equal(b2));
  EXPECT_FALSE(b3.equal(b2));
}

TEST(ObjectTest, Long) {
  LongObject b(0);
  LongObject b2(0);
  LongObject b3(1);

  EXPECT_EQ(b.toBinary(), absl::nullopt);
  EXPECT_EQ(b.toInteger(), absl::nullopt);
  EXPECT_EQ(b.toDouble(), absl::nullopt);
  EXPECT_EQ(b.toDate(), absl::nullopt);
  EXPECT_EQ(b.toTypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedMap(), absl::nullopt);
  EXPECT_EQ(b.toTypedMap(), absl::nullopt);
  EXPECT_EQ(b.toClassInstance(), absl::nullopt);
  EXPECT_EQ(b.toRefDest(), absl::nullopt);
  EXPECT_TRUE(b.toDebugString().size() > 0);

  EXPECT_EQ(b.type(), Object::Type::Long);
  EXPECT_EQ(b.hash(), b2.hash());
  EXPECT_EQ(b.toLong().value(), 0);
  EXPECT_TRUE(b.hash() != b3.hash());
  EXPECT_TRUE(b.equal(b2));
  EXPECT_FALSE(b3.equal(b2));
}

TEST(ObjectTest, Date) {
  DateObject b(std::chrono::milliseconds(100));
  DateObject b2(std::chrono::milliseconds(100));
  DateObject b3(std::chrono::milliseconds(200));

  EXPECT_EQ(b.toBinary(), absl::nullopt);
  EXPECT_EQ(b.toInteger(), absl::nullopt);
  EXPECT_EQ(b.toDouble(), absl::nullopt);
  EXPECT_EQ(b.toLong(), absl::nullopt);
  EXPECT_EQ(b.toTypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedMap(), absl::nullopt);
  EXPECT_EQ(b.toTypedMap(), absl::nullopt);
  EXPECT_EQ(b.toClassInstance(), absl::nullopt);
  EXPECT_EQ(b.toRefDest(), absl::nullopt);
  EXPECT_TRUE(b.toDebugString().size() > 0);

  EXPECT_EQ(b.type(), Object::Type::Date);
  EXPECT_EQ(b.hash(), b2.hash());
  EXPECT_EQ(b.toDate().value().get().count(), 100);
  EXPECT_TRUE(b.hash() != b3.hash());
  EXPECT_TRUE(b.equal(b2));
  EXPECT_FALSE(b3.equal(b2));
}

TEST(ObjectTest, String) {
  StringObject b(absl::string_view("test"));
  StringObject b2(absl::string_view("test"));
  StringObject b3(absl::string_view("test1"));

  EXPECT_EQ(b.toBinary(), absl::nullopt);
  EXPECT_EQ(b.toInteger(), absl::nullopt);
  EXPECT_EQ(b.toDouble(), absl::nullopt);
  EXPECT_EQ(b.toDate(), absl::nullopt);
  EXPECT_EQ(b.toLong(), absl::nullopt);
  EXPECT_EQ(b.toTypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedList(), absl::nullopt);
  EXPECT_EQ(b.toUntypedMap(), absl::nullopt);
  EXPECT_EQ(b.toTypedMap(), absl::nullopt);
  EXPECT_EQ(b.toClassInstance(), absl::nullopt);
  EXPECT_EQ(b.toRefDest(), absl::nullopt);
  EXPECT_TRUE(b.toDebugString().size() > 0);

  EXPECT_EQ(b.type(), Object::Type::String);
  EXPECT_EQ(b.hash(), b2.hash());
  EXPECT_TRUE(b.toString().value().get() == "test");
  EXPECT_TRUE(b.hash() != b3.hash());
  EXPECT_TRUE(b.equal(b2));
  EXPECT_FALSE(b3.equal(b2));
}

TEST(ObjectTest, Null) {
  NullObject p;
  NullObject p2;
  EXPECT_EQ(p.toDebugString(), std::string("Type: Null"));
  EXPECT_EQ(p.type(), Object::Type::Null);
  EXPECT_TRUE(p.equal(p2));
  EXPECT_EQ(p.hash(), p2.hash());
}

TEST(ObjectTest, Ref) {
  StringObject r(absl::string_view("ref"));
  RefObject p(&r);
  RefObject p2(&r);
  EXPECT_TRUE(p.toDebugString().size() > 0);
  EXPECT_EQ(p.type(), Object::Type::Ref);
  EXPECT_TRUE(p.equal(p2));
  EXPECT_EQ(p.hash(), p2.hash());
  EXPECT_TRUE(p.toRefDest().value()->toString().value().get() == "ref");
}

TEST(ObjectTest, UntypedList) {
  Object::UntypedList untyped_list1;
  Object::UntypedList untyped_list2;
  Object::UntypedList untyped_list3;

  untyped_list1.push_back(
      std::make_unique<StringObject>(absl::string_view("obj1")));
  untyped_list1.push_back(std::make_unique<IntegerObject>(1));
  untyped_list1.push_back(std::make_unique<BooleanObject>(true));

  untyped_list2.push_back(
      std::make_unique<StringObject>(absl::string_view("obj1")));
  untyped_list2.push_back(std::make_unique<IntegerObject>(1));
  untyped_list2.push_back(std::make_unique<BooleanObject>(true));

  untyped_list3.push_back(
      std::make_unique<StringObject>(absl::string_view("obj1")));
  untyped_list3.push_back(std::make_unique<IntegerObject>(1));

  UntypedListObject p(std::move(untyped_list1));
  UntypedListObject p2(std::move(untyped_list2));
  UntypedListObject p3(std::move(untyped_list3));

  EXPECT_NE(p.toUntypedList(), absl::nullopt);
  EXPECT_TRUE(p.toDebugString().size() > 0);

  EXPECT_EQ(p.type(), Object::Type::UntypedList);
  EXPECT_EQ(p.hash(), p2.hash());
  EXPECT_TRUE(p.equal(p2));
  EXPECT_FALSE(p.equal(p3));
  EXPECT_TRUE(p.toUntypedList().value().get().size() == 3);
  UntypedListObject p4;
  p4.emplace_back(std::make_unique<IntegerObject>(1));
  p4.emplace_back(std::make_unique<BooleanObject>(true));
  EXPECT_TRUE(p4.get(0)->toInteger().value() == 1);
  EXPECT_TRUE(p4.get(1)->toBoolean().value());
  EXPECT_TRUE(p4.get(2) == nullptr);
}

TEST(ObjectTest, TypedList) {
  Object::UntypedList untyped_list1;
  Object::UntypedList untyped_list2;
  Object::UntypedList untyped_list3;
  untyped_list1.push_back(
      std::make_unique<StringObject>(absl::string_view("obj1")));
  untyped_list1.push_back(std::make_unique<IntegerObject>(1));
  untyped_list1.push_back(std::make_unique<BooleanObject>(true));

  untyped_list2.push_back(
      std::make_unique<StringObject>(absl::string_view("obj1")));
  untyped_list2.push_back(std::make_unique<IntegerObject>(1));
  untyped_list2.push_back(std::make_unique<BooleanObject>(true));

  untyped_list3.push_back(
      std::make_unique<StringObject>(absl::string_view("obj1")));
  untyped_list3.push_back(std::make_unique<IntegerObject>(1));

  TypedListObject p(std::string("typ1"), std::move(untyped_list1));
  TypedListObject p2(std::string("typ2"), std::move(untyped_list2));
  TypedListObject p3(std::string("typ1"), std::move(untyped_list3));

  EXPECT_TRUE(p.toDebugString().size() > 0);

  EXPECT_EQ(p.type(), Object::Type::TypedList);
  EXPECT_NE(p.hash(), p2.hash());
  EXPECT_FALSE(p.equal(p2));
  EXPECT_FALSE(p.equal(p3));
  EXPECT_FALSE(p2.equal(p3));
  EXPECT_TRUE(p.toTypedList().value().get().values_.size() == 3);

  TypedListObject p4;
  p4.emplace_back(std::make_unique<IntegerObject>(1));
  p4.emplace_back(std::make_unique<BooleanObject>(true));
  EXPECT_TRUE(p4.get(0)->toInteger().value() == 1);
  EXPECT_TRUE(p4.get(1)->toBoolean().value());
  EXPECT_TRUE(p4.get(2) == nullptr);
}

TEST(ObjectTest, UntypedMap) {
  Object::UntypedMap untyped_map1;
  Object::UntypedMap untyped_map2;
  untyped_map1.emplace(
      std::make_pair(std::make_unique<StringObject>(absl::string_view("key1")),
                     std::make_unique<IntegerObject>(1)));

  UntypedMapObject p(std::move(untyped_map1));
  EXPECT_EQ(p.type(), Object::Type::UntypedMap);
  EXPECT_EQ(p.toUntypedMap().value().get().size(), 1);
  EXPECT_TRUE(p.toDebugString().size() > 0);

  untyped_map2.emplace(
      std::make_pair(std::make_unique<StringObject>(absl::string_view("key1")),
                     std::make_unique<IntegerObject>(1)));

  UntypedMapObject p2(std::move(untyped_map2));
  EXPECT_EQ(p2.toUntypedMap().value().get().size(), 1);

  EXPECT_EQ(p.hash(), p2.hash());
  EXPECT_TRUE(p.equal(p2));

  // heterogeneous lookup by string view.
  EXPECT_EQ(p.toUntypedMap()
                .value()
                .get()
                .find(absl::string_view("key1"))
                ->second->toInteger()
                .value()
                .get(),
            1);
  EXPECT_EQ(p2.toUntypedMap()
                .value()
                .get()
                .find("key1")
                ->second->toInteger()
                .value()
                .get(),
            1);
}

TEST(ObjectTest, TypedMap) {
  Object::TypedMap map1;
  Object::TypedMap map2;
  Object::TypedMap map3;

  map1.type_name_ = std::string("type1");
  map1.field_name_and_value_.emplace(
      std::make_pair(std::make_unique<StringObject>(absl::string_view("key1")),
                     std::make_unique<IntegerObject>(1)));
  TypedMapObject p(std::move(map1));
  EXPECT_TRUE(p.toDebugString().size() > 0);
  EXPECT_TRUE(p.get("key1")->toInteger().value() == 1);

  EXPECT_EQ(p.type(), Object::Type::TypedMap);
  EXPECT_EQ(p.toTypedMap().value().get().field_name_and_value_.size(), 1);

  map2.type_name_ = std::string("type1");
  map2.field_name_and_value_.emplace(
      std::make_pair(std::make_unique<StringObject>(absl::string_view("key1")),
                     std::make_unique<IntegerObject>(2)));

  TypedMapObject p2(std::move(map2));

  map3.type_name_ = std::string("type2");
  map3.field_name_and_value_.emplace(
      std::make_pair(std::make_unique<StringObject>(absl::string_view("key1")),
                     std::make_unique<IntegerObject>(2)));

  TypedMapObject p3(std::move(map3));
  EXPECT_TRUE(p3.get("key1")->toInteger().value() == 2);

  TypedMapObject p4;
  p4.emplace(std::make_unique<StringObject>(absl::string_view("key2")),
             std::make_unique<StringObject>(absl::string_view("key3")));
  EXPECT_TRUE(p4.get("key2")->toString().value().get() == std::string("key3"));

  // p and p2 have the same hash value because they have the same type and
  // element size,
  //  but they are not actually the same.
  EXPECT_EQ(p.hash(), p2.hash());
  EXPECT_FALSE(p.equal(p2));

  EXPECT_NE(p.hash(), p3.hash());
  EXPECT_NE(p2.hash(), p3.hash());

  EXPECT_FALSE(p.equal(p3));
  EXPECT_FALSE(p2.equal(p3));
}

TEST(ObjectTest, ClassInstance) {
  Object::ClassInstance instance1;
  Object::ClassInstance instance2;
  Object::ClassInstance instance3;

  Object::RawDefinitionSharedPtr d1 = std::make_shared<Object::RawDefinition>();
  d1->type_ = "type1";
  Object::RawDefinitionSharedPtr d2 = std::make_shared<Object::RawDefinition>();
  d2->type_ = "type2";

  instance1.def_ = d1;
  instance2.def_ = d1;
  instance3.def_ = d2;

  ClassInstanceObject p1(std::move(instance1));
  ClassInstanceObject p2(std::move(instance2));
  ClassInstanceObject p3(std::move(instance3));

  EXPECT_TRUE(p1.toDebugString().size() > 0);

  EXPECT_EQ(p1.hash(), p2.hash());
  EXPECT_NE(p1.hash(), p3.hash());
  EXPECT_NE(p2.hash(), p3.hash());
  EXPECT_TRUE(p1.equal(p2));
  EXPECT_FALSE(p1.equal(p3));
  EXPECT_FALSE(p2.equal(p3));
}

TEST(ObjectTest, AsType) {
  BooleanObject b(true);
  Object* obj_b = &b;
  auto convert_after = obj_b->asType<BooleanObject>();
  EXPECT_EQ(convert_after.type(), Object::Type::Boolean);
  EXPECT_EQ(convert_after.toBoolean().value(), true);
}

TEST(ObjectTest, Iterator) {
  StringObject b(absl::string_view("test"));
  const StringObject const_b(absl::string_view("test"));
  std::string actual;
  for (const auto& i : b) {
    actual.push_back(i);
  }

  std::string const_actual;
  for (const auto& i : const_b) {
    const_actual.push_back(i);
  }

  EXPECT_EQ(const_actual, const_b.toString().value().get());
}

}  // namespace Hessian2
