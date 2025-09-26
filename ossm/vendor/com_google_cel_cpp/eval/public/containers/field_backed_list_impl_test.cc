#include "eval/public/containers/field_backed_list_impl.h"

#include <memory>
#include <string>

#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using ::testing::Eq;
using ::testing::DoubleEq;

using testutil::EqualsProto;

// Helper method. Creates simple pipeline containing Select step and runs it.
std::unique_ptr<CelList> CreateList(const TestMessage* message,
                                    const std::string& field,
                                    google::protobuf::Arena* arena) {
  const google::protobuf::FieldDescriptor* field_desc =
      message->GetDescriptor()->FindFieldByName(field);

  return std::make_unique<FieldBackedListImpl>(message, field_desc, arena);
}

TEST(FieldBackedListImplTest, BoolDatatypeTest) {
  TestMessage message;
  message.add_bool_list(true);
  message.add_bool_list(false);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "bool_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_EQ((*cel_list)[0].BoolOrDie(), true);
  EXPECT_EQ((*cel_list)[1].BoolOrDie(), false);
}

TEST(FieldBackedListImplTest, TestLength0) {
  TestMessage message;

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "int32_list", &arena);

  ASSERT_EQ(cel_list->size(), 0);
}

TEST(FieldBackedListImplTest, TestLength1) {
  TestMessage message;
  message.add_int32_list(1);
  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "int32_list", &arena);

  ASSERT_EQ(cel_list->size(), 1);
  EXPECT_EQ((*cel_list)[0].Int64OrDie(), 1);
}

TEST(FieldBackedListImplTest, TestLength100000) {
  TestMessage message;

  const int kLen = 100000;

  for (int i = 0; i < kLen; i++) {
    message.add_int32_list(i);
  }
  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "int32_list", &arena);

  ASSERT_EQ(cel_list->size(), kLen);
  for (int i = 0; i < kLen; i++) {
    EXPECT_EQ((*cel_list)[i].Int64OrDie(), i);
  }
}

TEST(FieldBackedListImplTest, Int32DatatypeTest) {
  TestMessage message;
  message.add_int32_list(1);
  message.add_int32_list(2);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "int32_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_EQ((*cel_list)[0].Int64OrDie(), 1);
  EXPECT_EQ((*cel_list)[1].Int64OrDie(), 2);
}

TEST(FieldBackedListImplTest, Int64DatatypeTest) {
  TestMessage message;
  message.add_int64_list(1);
  message.add_int64_list(2);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "int64_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_EQ((*cel_list)[0].Int64OrDie(), 1);
  EXPECT_EQ((*cel_list)[1].Int64OrDie(), 2);
}

TEST(FieldBackedListImplTest, Uint32DatatypeTest) {
  TestMessage message;
  message.add_uint32_list(1);
  message.add_uint32_list(2);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "uint32_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_EQ((*cel_list)[0].Uint64OrDie(), 1);
  EXPECT_EQ((*cel_list)[1].Uint64OrDie(), 2);
}

TEST(FieldBackedListImplTest, Uint64DatatypeTest) {
  TestMessage message;
  message.add_uint64_list(1);
  message.add_uint64_list(2);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "uint64_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_EQ((*cel_list)[0].Uint64OrDie(), 1);
  EXPECT_EQ((*cel_list)[1].Uint64OrDie(), 2);
}

TEST(FieldBackedListImplTest, FloatDatatypeTest) {
  TestMessage message;
  message.add_float_list(1);
  message.add_float_list(2);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "float_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_THAT((*cel_list)[0].DoubleOrDie(), DoubleEq(1));
  EXPECT_THAT((*cel_list)[1].DoubleOrDie(), DoubleEq(2));
}

TEST(FieldBackedListImplTest, DoubleDatatypeTest) {
  TestMessage message;
  message.add_double_list(1);
  message.add_double_list(2);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "double_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_THAT((*cel_list)[0].DoubleOrDie(), DoubleEq(1));
  EXPECT_THAT((*cel_list)[1].DoubleOrDie(), DoubleEq(2));
}

TEST(FieldBackedListImplTest, StringDatatypeTest) {
  TestMessage message;
  message.add_string_list("1");
  message.add_string_list("2");

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "string_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_EQ((*cel_list)[0].StringOrDie().value(), "1");
  EXPECT_EQ((*cel_list)[1].StringOrDie().value(), "2");
}

TEST(FieldBackedListImplTest, BytesDatatypeTest) {
  TestMessage message;
  message.add_bytes_list("1");
  message.add_bytes_list("2");

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "bytes_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_EQ((*cel_list)[0].BytesOrDie().value(), "1");
  EXPECT_EQ((*cel_list)[1].BytesOrDie().value(), "2");
}

TEST(FieldBackedListImplTest, MessageDatatypeTest) {
  TestMessage message;
  TestMessage* msg1 = message.add_message_list();
  TestMessage* msg2 = message.add_message_list();

  msg1->set_string_value("1");
  msg2->set_string_value("2");

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "message_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_THAT(*msg1, EqualsProto(*((*cel_list)[0].MessageOrDie())));
  EXPECT_THAT(*msg2, EqualsProto(*((*cel_list)[1].MessageOrDie())));
}

TEST(FieldBackedListImplTest, EnumDatatypeTest) {
  TestMessage message;

  message.add_enum_list(TestMessage::TEST_ENUM_1);
  message.add_enum_list(TestMessage::TEST_ENUM_2);

  google::protobuf::Arena arena;

  auto cel_list = CreateList(&message, "enum_list", &arena);

  ASSERT_EQ(cel_list->size(), 2);

  EXPECT_THAT((*cel_list)[0].Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
  EXPECT_THAT((*cel_list)[1].Int64OrDie(), Eq(TestMessage::TEST_ENUM_2));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
