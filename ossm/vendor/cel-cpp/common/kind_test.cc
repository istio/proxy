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

#include "common/kind.h"

#include <limits>
#include <type_traits>

#include "common/type_kind.h"
#include "common/value_kind.h"
#include "internal/testing.h"

namespace cel {
namespace {

static_assert(std::is_same_v<std::underlying_type_t<TypeKind>,
                             std::underlying_type_t<ValueKind>>,
              "TypeKind and ValueKind must have the same underlying type");

TEST(Kind, ToString) {
  EXPECT_EQ(KindToString(Kind::kError), "*error*");
  EXPECT_EQ(KindToString(Kind::kNullType), "null_type");
  EXPECT_EQ(KindToString(Kind::kDyn), "dyn");
  EXPECT_EQ(KindToString(Kind::kAny), "any");
  EXPECT_EQ(KindToString(Kind::kType), "type");
  EXPECT_EQ(KindToString(Kind::kBool), "bool");
  EXPECT_EQ(KindToString(Kind::kInt), "int");
  EXPECT_EQ(KindToString(Kind::kUint), "uint");
  EXPECT_EQ(KindToString(Kind::kDouble), "double");
  EXPECT_EQ(KindToString(Kind::kString), "string");
  EXPECT_EQ(KindToString(Kind::kBytes), "bytes");
  EXPECT_EQ(KindToString(Kind::kDuration), "duration");
  EXPECT_EQ(KindToString(Kind::kTimestamp), "timestamp");
  EXPECT_EQ(KindToString(Kind::kList), "list");
  EXPECT_EQ(KindToString(Kind::kMap), "map");
  EXPECT_EQ(KindToString(Kind::kStruct), "struct");
  EXPECT_EQ(KindToString(Kind::kUnknown), "*unknown*");
  EXPECT_EQ(KindToString(Kind::kOpaque), "*opaque*");
  EXPECT_EQ(KindToString(Kind::kBoolWrapper), "google.protobuf.BoolValue");
  EXPECT_EQ(KindToString(Kind::kIntWrapper), "google.protobuf.Int64Value");
  EXPECT_EQ(KindToString(Kind::kUintWrapper), "google.protobuf.UInt64Value");
  EXPECT_EQ(KindToString(Kind::kDoubleWrapper), "google.protobuf.DoubleValue");
  EXPECT_EQ(KindToString(Kind::kStringWrapper), "google.protobuf.StringValue");
  EXPECT_EQ(KindToString(Kind::kBytesWrapper), "google.protobuf.BytesValue");
  EXPECT_EQ(KindToString(static_cast<Kind>(std::numeric_limits<int>::max())),
            "*error*");
}

TEST(Kind, TypeKindRoundtrip) {
  EXPECT_EQ(TypeKindToKind(KindToTypeKind(Kind::kBool)), Kind::kBool);
}

TEST(Kind, ValueKindRoundtrip) {
  EXPECT_EQ(ValueKindToKind(KindToValueKind(Kind::kBool)), Kind::kBool);
}

TEST(Kind, IsTypeKind) {
  EXPECT_TRUE(KindIsTypeKind(Kind::kBool));
  EXPECT_TRUE(KindIsTypeKind(Kind::kAny));
  EXPECT_TRUE(KindIsTypeKind(Kind::kDyn));
}

TEST(Kind, IsValueKind) {
  EXPECT_TRUE(KindIsValueKind(Kind::kBool));
  EXPECT_FALSE(KindIsValueKind(Kind::kAny));
  EXPECT_FALSE(KindIsValueKind(Kind::kDyn));
}

TEST(Kind, Equality) {
  EXPECT_EQ(Kind::kBool, TypeKind::kBool);
  EXPECT_EQ(TypeKind::kBool, Kind::kBool);

  EXPECT_EQ(Kind::kBool, ValueKind::kBool);
  EXPECT_EQ(ValueKind::kBool, Kind::kBool);

  EXPECT_NE(Kind::kBool, TypeKind::kInt);
  EXPECT_NE(TypeKind::kInt, Kind::kBool);

  EXPECT_NE(Kind::kBool, ValueKind::kInt);
  EXPECT_NE(ValueKind::kInt, Kind::kBool);
}

TEST(TypeKind, ToString) {
  EXPECT_EQ(TypeKindToString(TypeKind::kBool), KindToString(Kind::kBool));
}

TEST(ValueKind, ToString) {
  EXPECT_EQ(ValueKindToString(ValueKind::kBool), KindToString(Kind::kBool));
}

}  // namespace
}  // namespace cel
