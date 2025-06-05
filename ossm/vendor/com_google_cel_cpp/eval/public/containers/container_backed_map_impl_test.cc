#include "eval/public/containers/container_backed_map_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "eval/public/cel_value.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Not;

TEST(ContainerBackedMapImplTest, TestMapInt64) {
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateInt64(2), CelValue::CreateInt64(3)}};

  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();

  ASSERT_THAT(cel_map, Not(IsNull()));

  EXPECT_THAT(cel_map->size(), Eq(2));

  // Test lookup with key == 1 ( should succeed )
  auto lookup1 = (*cel_map)[CelValue::CreateInt64(1)];

  ASSERT_TRUE(lookup1);

  CelValue cel_value = lookup1.value();

  ASSERT_TRUE(cel_value.IsInt64());
  EXPECT_THAT(cel_value.Int64OrDie(), 2);

  // Test lookup with key == 1, different type ( should fail )
  auto lookup2 = (*cel_map)[CelValue::CreateUint64(1)];

  ASSERT_FALSE(lookup2);

  // Test lookup with key == 3 ( should fail )
  auto lookup3 = (*cel_map)[CelValue::CreateInt64(3)];

  ASSERT_FALSE(lookup3);
}

TEST(ContainerBackedMapImplTest, TestMapUint64) {
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateUint64(1), CelValue::CreateInt64(2)},
      {CelValue::CreateUint64(2), CelValue::CreateInt64(3)}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();

  ASSERT_THAT(cel_map, Not(IsNull()));

  EXPECT_THAT(cel_map->size(), Eq(2));

  // Test lookup with key == 1 ( should succeed )
  auto lookup1 = (*cel_map)[CelValue::CreateUint64(1)];

  ASSERT_TRUE(lookup1);

  CelValue cel_value = lookup1.value();

  ASSERT_TRUE(cel_value.IsInt64());
  EXPECT_THAT(cel_value.Int64OrDie(), 2);

  // Test lookup with key == 1, different type ( should fail )
  auto lookup2 = (*cel_map)[CelValue::CreateInt64(1)];

  ASSERT_FALSE(lookup2);

  // Test lookup with key == 3 ( should fail )
  auto lookup3 = (*cel_map)[CelValue::CreateUint64(3)];

  ASSERT_FALSE(lookup3);
}

TEST(ContainerBackedMapImplTest, TestMapString) {
  const std::string kKey1 = "1";
  const std::string kKey2 = "2";
  const std::string kKey3 = "3";

  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateString(&kKey1), CelValue::CreateInt64(2)},
      {CelValue::CreateString(&kKey2), CelValue::CreateInt64(3)}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();

  ASSERT_THAT(cel_map, Not(IsNull()));

  EXPECT_THAT(cel_map->size(), Eq(2));

  // Test lookup with key == 1 ( should succeed )
  auto lookup1 = (*cel_map)[CelValue::CreateString(&kKey1)];

  ASSERT_TRUE(lookup1);

  CelValue cel_value = lookup1.value();

  ASSERT_TRUE(cel_value.IsInt64());
  EXPECT_THAT(cel_value.Int64OrDie(), 2);

  // Test lookup with different type ( should fail )
  auto lookup2 = (*cel_map)[CelValue::CreateInt64(1)];

  ASSERT_FALSE(lookup2);

  // Test lookup with key3 ( should fail )
  auto lookup3 = (*cel_map)[CelValue::CreateString(&kKey3)];

  ASSERT_FALSE(lookup3);
}

TEST(CelMapBuilder, TestMapString) {
  const std::string kKey1 = "1";
  const std::string kKey2 = "2";
  const std::string kKey3 = "3";

  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateString(&kKey1), CelValue::CreateInt64(2)},
      {CelValue::CreateString(&kKey2), CelValue::CreateInt64(3)}};
  CelMapBuilder builder;
  ASSERT_OK(
      builder.Add(CelValue::CreateString(&kKey1), CelValue::CreateInt64(2)));
  ASSERT_OK(
      builder.Add(CelValue::CreateString(&kKey2), CelValue::CreateInt64(3)));

  CelMap* cel_map = &builder;

  ASSERT_THAT(cel_map, Not(IsNull()));

  EXPECT_THAT(cel_map->size(), Eq(2));

  // Test lookup with key == 1 ( should succeed )
  auto lookup1 = (*cel_map)[CelValue::CreateString(&kKey1)];

  ASSERT_TRUE(lookup1);

  CelValue cel_value = lookup1.value();

  ASSERT_TRUE(cel_value.IsInt64());
  EXPECT_THAT(cel_value.Int64OrDie(), 2);

  // Test lookup with different type ( should fail )
  auto lookup2 = (*cel_map)[CelValue::CreateInt64(1)];

  ASSERT_FALSE(lookup2);

  // Test lookup with key3 ( should fail )
  auto lookup3 = (*cel_map)[CelValue::CreateString(&kKey3)];

  ASSERT_FALSE(lookup3);
}

TEST(CelMapBuilder, RepeatKeysFail) {
  const std::string kKey1 = "1";
  const std::string kKey2 = "2";

  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateString(&kKey1), CelValue::CreateInt64(2)},
      {CelValue::CreateString(&kKey2), CelValue::CreateInt64(3)}};
  CelMapBuilder builder;
  ASSERT_OK(
      builder.Add(CelValue::CreateString(&kKey1), CelValue::CreateInt64(2)));
  ASSERT_OK(
      builder.Add(CelValue::CreateString(&kKey2), CelValue::CreateInt64(3)));
  EXPECT_THAT(
      builder.Add(CelValue::CreateString(&kKey2), CelValue::CreateInt64(3)),
      StatusIs(absl::StatusCode::kInvalidArgument, "duplicate map keys"));
}

}  // namespace

}  // namespace google::api::expr::runtime
