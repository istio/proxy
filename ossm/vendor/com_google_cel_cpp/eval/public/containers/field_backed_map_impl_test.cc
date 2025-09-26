#include "eval/public/containers/field_backed_map_impl.h"

#include <array>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::UnorderedPointwise;

// Test factory for FieldBackedMaps from message and field name.
std::unique_ptr<FieldBackedMapImpl> CreateMap(const TestMessage* message,
                                              const std::string& field,
                                              google::protobuf::Arena* arena) {
  const google::protobuf::FieldDescriptor* field_desc =
      message->GetDescriptor()->FindFieldByName(field);

  return std::make_unique<FieldBackedMapImpl>(message, field_desc, arena);
}

TEST(FieldBackedMapImplTest, BadKeyTypeTest) {
  TestMessage message;
  google::protobuf::Arena arena;
  constexpr std::array<absl::string_view, 6> map_types = {
      "int64_int32_map", "uint64_int32_map", "string_int32_map",
      "bool_int32_map",  "int32_int32_map",  "uint32_uint32_map",
  };

  for (auto map_type : map_types) {
    auto cel_map = CreateMap(&message, std::string(map_type), &arena);
    // Look up a boolean key. This should result in an error for both the
    // presence test and the value lookup.
    auto result = cel_map->Has(CelValue::CreateNull());
    EXPECT_FALSE(result.ok());
    EXPECT_THAT(result.status().code(), Eq(absl::StatusCode::kInvalidArgument));

    EXPECT_FALSE(result.ok());
    EXPECT_THAT(result.status().code(), Eq(absl::StatusCode::kInvalidArgument));

    auto lookup = (*cel_map)[CelValue::CreateNull()];
    EXPECT_TRUE(lookup.has_value());
    EXPECT_TRUE(lookup->IsError());
    EXPECT_THAT(lookup->ErrorOrDie()->code(),
                Eq(absl::StatusCode::kInvalidArgument));
  }
}

TEST(FieldBackedMapImplTest, Int32KeyTest) {
  TestMessage message;
  auto field_map = message.mutable_int32_int32_map();
  (*field_map)[0] = 1;
  (*field_map)[1] = 2;

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "int32_int32_map", &arena);

  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(0)]->Int64OrDie(), 1);
  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(1)]->Int64OrDie(), 2);
  EXPECT_TRUE(cel_map->Has(CelValue::CreateInt64(1)).value_or(false));

  // Look up nonexistent key
  EXPECT_FALSE((*cel_map)[CelValue::CreateInt64(3)].has_value());
  EXPECT_FALSE(cel_map->Has(CelValue::CreateInt64(3)).value_or(true));
}

TEST(FieldBackedMapImplTest, Int32KeyOutOfRangeTest) {
  TestMessage message;
  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "int32_int32_map", &arena);

  // Look up keys out of int32 range
  auto result = cel_map->Has(
      CelValue::CreateInt64(std::numeric_limits<int32_t>::max() + 1L));
  EXPECT_THAT(result.status(),
              StatusIs(absl::StatusCode::kOutOfRange, HasSubstr("overflow")));

  result = cel_map->Has(
      CelValue::CreateInt64(std::numeric_limits<int32_t>::lowest() - 1L));
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().code(), Eq(absl::StatusCode::kOutOfRange));
}

TEST(FieldBackedMapImplTest, Int64KeyTest) {
  TestMessage message;
  auto field_map = message.mutable_int64_int32_map();
  (*field_map)[0] = 1;
  (*field_map)[1] = 2;

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "int64_int32_map", &arena);

  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(0)]->Int64OrDie(), 1);
  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(1)]->Int64OrDie(), 2);
  EXPECT_TRUE(cel_map->Has(CelValue::CreateInt64(1)).value_or(false));

  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateInt64(3)].has_value(), false);
}

TEST(FieldBackedMapImplTest, BoolKeyTest) {
  TestMessage message;
  auto field_map = message.mutable_bool_int32_map();
  (*field_map)[false] = 1;

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "bool_int32_map", &arena);

  EXPECT_EQ((*cel_map)[CelValue::CreateBool(false)]->Int64OrDie(), 1);
  EXPECT_TRUE(cel_map->Has(CelValue::CreateBool(false)).value_or(false));
  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateBool(true)].has_value(), false);

  (*field_map)[true] = 2;
  EXPECT_EQ((*cel_map)[CelValue::CreateBool(true)]->Int64OrDie(), 2);
}

TEST(FieldBackedMapImplTest, Uint32KeyTest) {
  TestMessage message;
  auto field_map = message.mutable_uint32_uint32_map();
  (*field_map)[0] = 1u;
  (*field_map)[1] = 2u;

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "uint32_uint32_map", &arena);

  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(0)]->Uint64OrDie(), 1UL);
  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(1)]->Uint64OrDie(), 2UL);
  EXPECT_TRUE(cel_map->Has(CelValue::CreateUint64(1)).value_or(false));

  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(3)].has_value(), false);
  EXPECT_EQ(cel_map->Has(CelValue::CreateUint64(3)).value_or(true), false);
}

TEST(FieldBackedMapImplTest, Uint32KeyOutOfRangeTest) {
  TestMessage message;
  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "uint32_uint32_map", &arena);

  // Look up keys out of uint32 range
  auto result = cel_map->Has(
      CelValue::CreateUint64(std::numeric_limits<uint32_t>::max() + 1UL));
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().code(), Eq(absl::StatusCode::kOutOfRange));
}

TEST(FieldBackedMapImplTest, Uint64KeyTest) {
  TestMessage message;
  auto field_map = message.mutable_uint64_int32_map();
  (*field_map)[0] = 1;
  (*field_map)[1] = 2;

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "uint64_int32_map", &arena);

  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(0)]->Int64OrDie(), 1);
  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(1)]->Int64OrDie(), 2);
  EXPECT_TRUE(cel_map->Has(CelValue::CreateUint64(1)).value_or(false));

  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateUint64(3)].has_value(), false);
}

TEST(FieldBackedMapImplTest, StringKeyTest) {
  TestMessage message;
  auto field_map = message.mutable_string_int32_map();
  (*field_map)["test0"] = 1;
  (*field_map)["test1"] = 2;

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "string_int32_map", &arena);

  std::string test0 = "test0";
  std::string test1 = "test1";
  std::string test_notfound = "test_notfound";

  EXPECT_EQ((*cel_map)[CelValue::CreateString(&test0)]->Int64OrDie(), 1);
  EXPECT_EQ((*cel_map)[CelValue::CreateString(&test1)]->Int64OrDie(), 2);
  EXPECT_TRUE(cel_map->Has(CelValue::CreateString(&test1)).value_or(false));

  // Look up nonexistent key
  EXPECT_EQ((*cel_map)[CelValue::CreateString(&test_notfound)].has_value(),
            false);
}

TEST(FieldBackedMapImplTest, EmptySizeTest) {
  TestMessage message;
  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "string_int32_map", &arena);
  EXPECT_EQ(cel_map->size(), 0);
}

TEST(FieldBackedMapImplTest, RepeatedAddTest) {
  TestMessage message;
  auto field_map = message.mutable_string_int32_map();
  (*field_map)["test0"] = 1;
  (*field_map)["test1"] = 2;
  (*field_map)["test0"] = 3;

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "string_int32_map", &arena);

  EXPECT_EQ(cel_map->size(), 2);
}

TEST(FieldBackedMapImplTest, KeyListTest) {
  TestMessage message;
  auto field_map = message.mutable_string_int32_map();
  std::vector<std::string> keys;
  std::vector<std::string> keys1;
  for (int i = 0; i < 100; i++) {
    keys.push_back(absl::StrCat("test", i));
    (*field_map)[keys.back()] = i;
  }

  google::protobuf::Arena arena;
  auto cel_map = CreateMap(&message, "string_int32_map", &arena);
  const CelList* key_list = cel_map->ListKeys().value();

  EXPECT_EQ(key_list->size(), 100);
  for (int i = 0; i < key_list->size(); i++) {
    keys1.push_back(std::string((*key_list)[i].StringOrDie().value()));
  }

  EXPECT_THAT(keys, UnorderedPointwise(Eq(), keys1));
}

}  // namespace
}  // namespace google::api::expr::runtime
