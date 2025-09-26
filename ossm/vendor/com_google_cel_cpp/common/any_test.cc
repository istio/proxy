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

#include "common/any.h"

#include <string>

#include "google/protobuf/any.pb.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(Any, Value) {
  google::protobuf::Any any;
  std::string scratch;
  SetAnyValueFromCord(&any, absl::Cord("Hello World!"));
  EXPECT_EQ(GetAnyValueAsCord(any), "Hello World!");
  EXPECT_EQ(GetAnyValueAsString(any), "Hello World!");
  EXPECT_EQ(GetAnyValueAsStringView(any, scratch), "Hello World!");
}

TEST(MakeTypeUrlWithPrefix, Basic) {
  EXPECT_EQ(MakeTypeUrlWithPrefix("foo", "bar.Baz"), "foo/bar.Baz");
  EXPECT_EQ(MakeTypeUrlWithPrefix("foo/", "bar.Baz"), "foo/bar.Baz");
}

TEST(MakeTypeUrl, Basic) {
  EXPECT_EQ(MakeTypeUrl("bar.Baz"), "type.googleapis.com/bar.Baz");
}

TEST(ParseTypeUrl, Valid) {
  EXPECT_TRUE(ParseTypeUrl("type.googleapis.com/bar.Baz"));
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com"));
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com/"));
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com/foo/"));
}

TEST(ParseTypeUrl, TypeName) {
  absl::string_view type_name;
  EXPECT_TRUE(ParseTypeUrl("type.googleapis.com/bar.Baz", &type_name));
  EXPECT_EQ(type_name, "bar.Baz");
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com", &type_name));
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com/", &type_name));
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com/foo/", &type_name));
}

TEST(ParseTypeUrl, PrefixAndTypeName) {
  absl::string_view prefix;
  absl::string_view type_name;
  EXPECT_TRUE(ParseTypeUrl("type.googleapis.com/bar.Baz", &prefix, &type_name));
  EXPECT_EQ(prefix, "type.googleapis.com/");
  EXPECT_EQ(type_name, "bar.Baz");
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com", &prefix, &type_name));
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com/", &prefix, &type_name));
  EXPECT_FALSE(ParseTypeUrl("type.googleapis.com/foo/", &prefix, &type_name));
}

}  // namespace
}  // namespace cel
