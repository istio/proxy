#include "eval/public/activation_bind_helper.h"

#include "absl/status/status.h"
#include "eval/public/activation.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using testutil::EqualsProto;

TEST(ActivationBindHelperTest, TestSingleBoolBind) {
  TestMessage message;
  message.set_bool_value(true);

  google::protobuf::Arena arena;

  Activation activation;

  ASSERT_OK(BindProtoToActivation(&message, &arena, &activation));

  auto result = activation.FindValue("bool_value", &arena);

  ASSERT_TRUE(result.has_value());

  CelValue value = result.value();

  ASSERT_TRUE(value.IsBool());
  EXPECT_EQ(value.BoolOrDie(), true);
}

TEST(ActivationBindHelperTest, TestSingleInt32Bind) {
  TestMessage message;
  message.set_int32_value(42);

  google::protobuf::Arena arena;

  Activation activation;

  ASSERT_OK(BindProtoToActivation(&message, &arena, &activation));

  auto result = activation.FindValue("int32_value", &arena);

  ASSERT_TRUE(result.has_value());

  CelValue value = result.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_EQ(value.Int64OrDie(), 42);
}

TEST(ActivationBindHelperTest, TestUnsetRepeatedIsEmptyList) {
  TestMessage message;

  google::protobuf::Arena arena;

  Activation activation;

  ASSERT_OK(BindProtoToActivation(&message, &arena, &activation));

  auto result = activation.FindValue("int32_list", &arena);

  ASSERT_TRUE(result.has_value());

  CelValue value = result.value();

  ASSERT_TRUE(value.IsList());
  EXPECT_TRUE(value.ListOrDie()->empty());
}

TEST(ActivationBindHelperTest, TestSkipUnsetFields) {
  TestMessage message;
  message.set_int32_value(42);

  google::protobuf::Arena arena;

  Activation activation;

  ASSERT_OK(BindProtoToActivation(&message, &arena, &activation,
                                  ProtoUnsetFieldOptions::kSkip));

  // Explicitly set field is unaffected.
  auto result = activation.FindValue("int32_value", &arena);

  ASSERT_TRUE(result.has_value());

  CelValue value = result.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_EQ(value.Int64OrDie(), 42);

  result = activation.FindValue("message_value", &arena);
  ASSERT_FALSE(result.has_value());
}

TEST(ActivationBindHelperTest, TestBindDefaultFields) {
  TestMessage message;
  message.set_int32_value(42);

  google::protobuf::Arena arena;

  Activation activation;

  ASSERT_OK(BindProtoToActivation(&message, &arena, &activation,
                                  ProtoUnsetFieldOptions::kBindDefault));

  auto result = activation.FindValue("int32_value", &arena);

  ASSERT_TRUE(result.has_value());

  CelValue value = result.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_EQ(value.Int64OrDie(), 42);

  result = activation.FindValue("message_value", &arena);
  ASSERT_TRUE(result.has_value());
  EXPECT_NE(nullptr, result->MessageOrDie());
  EXPECT_THAT(TestMessage::default_instance(),
              EqualsProto(*result->MessageOrDie()));
}

TEST(ActivationBindHelperTest, RejectsNullArena) {
  TestMessage message;
  message.set_bool_value(true);

  Activation activation;

  ASSERT_EQ(BindProtoToActivation(&message, /*arena=*/nullptr, &activation),
            absl::InvalidArgumentError(
                "arena must not be null for BindProtoToActivation."));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
