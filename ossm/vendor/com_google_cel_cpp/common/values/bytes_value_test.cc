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

#include <sstream>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::testing::An;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Optional;

using BytesValueTest = common_internal::ValueTest<>;

TEST_F(BytesValueTest, Kind) {
  EXPECT_EQ(BytesValue("foo").kind(), BytesValue::kKind);
  EXPECT_EQ(Value(BytesValue(absl::Cord("foo"))).kind(), BytesValue::kKind);
}

TEST_F(BytesValueTest, DebugString) {
  {
    std::ostringstream out;
    out << BytesValue("foo");
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
  {
    std::ostringstream out;
    out << BytesValue(absl::MakeFragmentedCord({"f", "o", "o"}));
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
  {
    std::ostringstream out;
    out << Value(BytesValue(absl::Cord("foo")));
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
}

TEST_F(BytesValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(BytesValue("foo").ConvertToJson(descriptor_pool(),
                                              message_factory(), message),
              IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(string_value: "Zm9v")pb"));
}

TEST_F(BytesValueTest, NativeValue) {
  std::string scratch;
  EXPECT_EQ(BytesValue("foo").NativeString(), "foo");
  EXPECT_EQ(BytesValue("foo").NativeString(scratch), "foo");
  EXPECT_EQ(BytesValue("foo").NativeCord(), "foo");
}

TEST_F(BytesValueTest, TryFlat) {
  EXPECT_THAT(BytesValue("foo").TryFlat(), Optional(Eq("foo")));
  EXPECT_THAT(
      BytesValue(absl::MakeFragmentedCord({"Hello, World!", "World, Hello!"}))
          .TryFlat(),
      Eq(absl::nullopt));
}

TEST_F(BytesValueTest, ToString) {
  EXPECT_EQ(BytesValue("foo").ToString(), "foo");
  EXPECT_EQ(BytesValue(absl::MakeFragmentedCord({"f", "o", "o"})).ToString(),
            "foo");
}

TEST_F(BytesValueTest, CopyToString) {
  std::string out;
  BytesValue("foo").CopyToString(&out);
  EXPECT_EQ(out, "foo");
  BytesValue(absl::MakeFragmentedCord({"f", "o", "o"})).CopyToString(&out);
  EXPECT_EQ(out, "foo");
}

TEST_F(BytesValueTest, AppendToString) {
  std::string out;
  BytesValue("foo").AppendToString(&out);
  EXPECT_EQ(out, "foo");
  BytesValue(absl::MakeFragmentedCord({"f", "o", "o"})).AppendToString(&out);
  EXPECT_EQ(out, "foofoo");
}

TEST_F(BytesValueTest, ToCord) {
  EXPECT_EQ(BytesValue("foo").ToCord(), "foo");
  EXPECT_EQ(BytesValue(absl::MakeFragmentedCord({"f", "o", "o"})).ToCord(),
            "foo");
}

TEST_F(BytesValueTest, CopyToCord) {
  absl::Cord out;
  BytesValue("foo").CopyToCord(&out);
  EXPECT_EQ(out, "foo");
  BytesValue(absl::MakeFragmentedCord({"f", "o", "o"})).CopyToCord(&out);
  EXPECT_EQ(out, "foo");
}

TEST_F(BytesValueTest, AppendToCord) {
  absl::Cord out;
  BytesValue("foo").AppendToCord(&out);
  EXPECT_EQ(out, "foo");
  BytesValue(absl::MakeFragmentedCord({"f", "o", "o"})).AppendToCord(&out);
  EXPECT_EQ(out, "foofoo");
}

TEST_F(BytesValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesValue("foo")),
            NativeTypeId::For<BytesValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(BytesValue(absl::Cord("foo")))),
            NativeTypeId::For<BytesValue>());
}

TEST_F(BytesValueTest, StringViewEquality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_TRUE(BytesValue("foo") == "foo");
  EXPECT_FALSE(BytesValue("foo") == "bar");

  EXPECT_TRUE("foo" == BytesValue("foo"));
  EXPECT_FALSE("bar" == BytesValue("foo"));
  // NOLINTEND(readability/check)
}

TEST_F(BytesValueTest, StringViewInequality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_FALSE(BytesValue("foo") != "foo");
  EXPECT_TRUE(BytesValue("foo") != "bar");

  EXPECT_FALSE("foo" != BytesValue("foo"));
  EXPECT_TRUE("bar" != BytesValue("foo"));
  // NOLINTEND(readability/check)
}

TEST_F(BytesValueTest, Comparison) {
  EXPECT_LT(BytesValue("bar"), BytesValue("foo"));
  EXPECT_FALSE(BytesValue("foo") < BytesValue("foo"));
  EXPECT_FALSE(BytesValue("foo") < BytesValue("bar"));
}

TEST_F(BytesValueTest, StringInputStream) {
  BytesValue value = BytesValue("foo");
  BytesValueInputStream stream(&value);
  const void* data;
  int size;
  absl::Cord cord;
  ASSERT_TRUE(stream.Next(&data, &size));
  EXPECT_THAT(data, NotNull());
  EXPECT_EQ(size, 3);
  EXPECT_EQ(stream.ByteCount(), 3);
  stream.BackUp(size);
  ASSERT_TRUE(stream.Skip(3));
  EXPECT_FALSE(stream.ReadCord(&cord, 3));
  EXPECT_FALSE(stream.Next(&data, &size));
}

TEST_F(BytesValueTest, CordInputStream) {
  BytesValue value = BytesValue(absl::Cord("foo"));
  BytesValueInputStream stream(&value);
  const void* data;
  int size;
  absl::Cord cord;
  ASSERT_TRUE(stream.Next(&data, &size));
  EXPECT_THAT(data, NotNull());
  EXPECT_EQ(size, 3);
  EXPECT_EQ(stream.ByteCount(), 3);
  stream.BackUp(size);
  ASSERT_TRUE(stream.Skip(3));
  EXPECT_FALSE(stream.ReadCord(&cord, 3));
  EXPECT_FALSE(stream.Next(&data, &size));
}

TEST_F(BytesValueTest, ArenaStringOutputStream) {
  BytesValue value = BytesValue("");
  {
    BytesValueOutputStream stream(value, arena());
    EXPECT_THAT(stream.AllowsAliasing(), An<bool>());
    EXPECT_EQ(stream.ByteCount(), 0);
    google::protobuf::Value value_proto;
    auto* struct_proto = value_proto.mutable_struct_value();
    (*struct_proto->mutable_fields())["foo"].set_string_value("bar");
    (*struct_proto->mutable_fields())["baz"].set_number_value(3.14159);
    ASSERT_TRUE(value_proto.SerializePartialToZeroCopyStream(&stream));
    EXPECT_EQ(std::move(stream).Consume(),
              value_proto.SerializePartialAsString());
  }
  {
    BytesValueOutputStream stream(value);
    EXPECT_EQ(std::move(stream).Consume(), "");
  }
}

TEST_F(BytesValueTest, StringOutputStream) {
  BytesValue value = BytesValue("");
  {
    BytesValueOutputStream stream(value);
    EXPECT_THAT(stream.AllowsAliasing(), An<bool>());
    EXPECT_EQ(stream.ByteCount(), 0);
    google::protobuf::Value value_proto;
    auto* struct_proto = value_proto.mutable_struct_value();
    (*struct_proto->mutable_fields())["foo"].set_string_value("bar");
    (*struct_proto->mutable_fields())["baz"].set_number_value(3.14159);
    ASSERT_TRUE(value_proto.SerializePartialToZeroCopyStream(&stream));
    EXPECT_EQ(std::move(stream).Consume(),
              value_proto.SerializePartialAsString());
  }
  {
    BytesValueOutputStream stream(value);
    EXPECT_EQ(std::move(stream).Consume(), "");
  }
}

TEST_F(BytesValueTest, CordOutputStream) {
  BytesValue value = BytesValue(absl::Cord());
  {
    BytesValueOutputStream stream(value);
    EXPECT_THAT(stream.AllowsAliasing(), An<bool>());
    EXPECT_EQ(stream.ByteCount(), 0);
    google::protobuf::Value value_proto;
    auto* struct_proto = value_proto.mutable_struct_value();
    (*struct_proto->mutable_fields())["foo"].set_string_value("bar");
    (*struct_proto->mutable_fields())["baz"].set_number_value(3.14159);
    ASSERT_TRUE(value_proto.SerializePartialToZeroCopyStream(&stream));
    EXPECT_EQ(std::move(stream).Consume(),
              value_proto.SerializePartialAsString());
  }
  {
    BytesValueOutputStream stream(value);
    EXPECT_EQ(std::move(stream).Consume(), "");
  }
}

}  // namespace
}  // namespace cel
