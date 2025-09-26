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

#include "common/typeinfo.h"

#include <cstring>
#include <sstream>

#include "absl/hash/hash_testing.h"
#include "absl/strings/str_cat.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

struct Type1 {};

struct Type2 {};

struct Type3 {};

TEST(TypeInfo, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {TypeInfo(), cel::TypeId<Type1>(), cel::TypeId<Type2>(),
       cel::TypeId<Type3>()}));
}

TEST(TypeInfo, Ostream) {
  std::ostringstream out;
  out << TypeInfo();
  EXPECT_THAT(out.str(), IsEmpty());
  out << cel::TypeId<Type1>();
  auto string = out.str();
  EXPECT_THAT(string, Not(IsEmpty()));
  EXPECT_THAT(string, SizeIs(std::strlen(string.c_str())));
}

TEST(TypeInfo, AbslStringify) {
  EXPECT_THAT(absl::StrCat(TypeInfo()), IsEmpty());
  EXPECT_THAT(absl::StrCat(cel::TypeId<Type1>()), Not(IsEmpty()));
}

struct TestType {};

}  // namespace

template <>
struct NativeTypeTraits<TestType> final {
  static TypeInfo Id(const TestType&) { return cel::TypeId<TestType>(); }
};

namespace {

TEST(TypeInfo, Of) {
  EXPECT_EQ(cel::TypeId(TestType()), cel::TypeId<TestType>());
}

}  // namespace

}  // namespace cel
