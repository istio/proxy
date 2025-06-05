// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/json.h"

#include "absl/hash/hash_testing.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

TEST(Json, DefaultConstructor) {
  EXPECT_THAT(Json(), VariantWith<JsonNull>(Eq(kJsonNull)));
}

TEST(Json, NullConstructor) {
  EXPECT_THAT(Json(kJsonNull), VariantWith<JsonNull>(Eq(kJsonNull)));
}

TEST(Json, FalseConstructor) {
  EXPECT_THAT(Json(false), VariantWith<JsonBool>(IsFalse()));
}

TEST(Json, TrueConstructor) {
  EXPECT_THAT(Json(true), VariantWith<JsonBool>(IsTrue()));
}

TEST(Json, NumberConstructor) {
  EXPECT_THAT(Json(1.0), VariantWith<JsonNumber>(1));
}

TEST(Json, StringConstructor) {
  EXPECT_THAT(Json(JsonString("foo")), VariantWith<JsonString>(Eq("foo")));
}

TEST(Json, ArrayConstructor) {
  EXPECT_THAT(Json(JsonArray()), VariantWith<JsonArray>(Eq(JsonArray())));
}

TEST(Json, ObjectConstructor) {
  EXPECT_THAT(Json(JsonObject()), VariantWith<JsonObject>(Eq(JsonObject())));
}

TEST(Json, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {Json(), Json(true), Json(1.0), Json(JsonString("foo")),
       Json(JsonArray()), Json(JsonObject())}));
}

TEST(JsonArrayBuilder, DefaultConstructor) {
  JsonArrayBuilder builder;
  EXPECT_TRUE(builder.empty());
  EXPECT_EQ(builder.size(), 0);
}

TEST(JsonArrayBuilder, OneOfEach) {
  JsonArrayBuilder builder;
  builder.reserve(6);
  builder.push_back(kJsonNull);
  builder.push_back(true);
  builder.push_back(1.0);
  builder.push_back(JsonString("foo"));
  builder.push_back(JsonArray());
  builder.push_back(JsonObject());
  EXPECT_FALSE(builder.empty());
  EXPECT_EQ(builder.size(), 6);
  EXPECT_THAT(builder, ElementsAre(kJsonNull, true, 1.0, JsonString("foo"),
                                   JsonArray(), JsonObject()));
  builder.pop_back();
  EXPECT_FALSE(builder.empty());
  EXPECT_EQ(builder.size(), 5);
  EXPECT_THAT(builder, ElementsAre(kJsonNull, true, 1.0, JsonString("foo"),
                                   JsonArray()));
  builder.clear();
  EXPECT_TRUE(builder.empty());
  EXPECT_EQ(builder.size(), 0);
}

TEST(JsonObjectBuilder, DefaultConstructor) {
  JsonObjectBuilder builder;
  EXPECT_TRUE(builder.empty());
  EXPECT_EQ(builder.size(), 0);
}

TEST(JsonObjectBuilder, OneOfEach) {
  JsonObjectBuilder builder;
  builder.reserve(6);
  builder.insert_or_assign(JsonString("foo"), kJsonNull);
  builder.insert_or_assign(JsonString("bar"), true);
  builder.insert_or_assign(JsonString("baz"), 1.0);
  builder.insert_or_assign(JsonString("qux"), JsonString("foo"));
  builder.insert_or_assign(JsonString("quux"), JsonArray());
  builder.insert_or_assign(JsonString("corge"), JsonObject());
  EXPECT_FALSE(builder.empty());
  EXPECT_EQ(builder.size(), 6);
  EXPECT_THAT(builder, UnorderedElementsAre(
                           std::make_pair(JsonString("foo"), kJsonNull),
                           std::make_pair(JsonString("bar"), true),
                           std::make_pair(JsonString("baz"), 1.0),
                           std::make_pair(JsonString("qux"), JsonString("foo")),
                           std::make_pair(JsonString("quux"), JsonArray()),
                           std::make_pair(JsonString("corge"), JsonObject())));
  builder.erase(JsonString("corge"));
  EXPECT_FALSE(builder.empty());
  EXPECT_EQ(builder.size(), 5);
  EXPECT_THAT(builder, UnorderedElementsAre(
                           std::make_pair(JsonString("foo"), kJsonNull),
                           std::make_pair(JsonString("bar"), true),
                           std::make_pair(JsonString("baz"), 1.0),
                           std::make_pair(JsonString("qux"), JsonString("foo")),
                           std::make_pair(JsonString("quux"), JsonArray())));
  builder.clear();
  EXPECT_TRUE(builder.empty());
  EXPECT_EQ(builder.size(), 0);
}

TEST(JsonInt, Basic) {
  EXPECT_THAT(JsonInt(1), VariantWith<JsonNumber>(1.0));
  EXPECT_THAT(JsonInt(std::numeric_limits<int64_t>::max()),
              VariantWith<JsonString>(
                  Eq(absl::StrCat(std::numeric_limits<int64_t>::max()))));
}

TEST(JsonUint, Basic) {
  EXPECT_THAT(JsonUint(1), VariantWith<JsonNumber>(1.0));
  EXPECT_THAT(JsonUint(std::numeric_limits<uint64_t>::max()),
              VariantWith<JsonString>(
                  Eq(absl::StrCat(std::numeric_limits<uint64_t>::max()))));
}

TEST(JsonBytes, Basic) {
  EXPECT_THAT(JsonBytes("foo"),
              VariantWith<JsonString>(Eq(absl::Base64Escape("foo"))));
  EXPECT_THAT(JsonBytes(absl::Cord("foo")),
              VariantWith<JsonString>(Eq(absl::Base64Escape("foo"))));
}

}  // namespace
}  // namespace cel::internal
