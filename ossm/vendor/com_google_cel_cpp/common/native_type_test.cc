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

#include "common/native_type.h"

#include <cstring>
#include <sstream>

#include "absl/hash/hash_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

struct Type1 {};

struct Type2 {};

struct Type3 {};

TEST(NativeTypeId, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {NativeTypeId(), NativeTypeId::For<Type1>(), NativeTypeId::For<Type2>(),
       NativeTypeId::For<Type3>()}));
}

TEST(NativeTypeId, DebugString) {
  std::ostringstream out;
  out << NativeTypeId();
  EXPECT_THAT(out.str(), IsEmpty());
  out << NativeTypeId::For<Type1>();
  auto string = out.str();
  EXPECT_THAT(string, Not(IsEmpty()));
  EXPECT_THAT(string, SizeIs(std::strlen(string.c_str())));
}

struct TestType {};

}  // namespace

template <>
struct NativeTypeTraits<TestType> final {
  static NativeTypeId Id(const TestType&) {
    return NativeTypeId::For<TestType>();
  }
};

namespace {

TEST(NativeTypeId, Of) {
  EXPECT_EQ(NativeTypeId::Of(TestType()), NativeTypeId::For<TestType>());
}

struct TrivialObject {};

TEST(NativeType, SkipDestructorTrivial) {
  EXPECT_TRUE(NativeType::SkipDestructor(TrivialObject{}));
}

struct NonTrivialObject {
  // Not "= default" on purpose to make this non-trivial.
  // NOLINTNEXTLINE(modernize-use-equals-default)
  ~NonTrivialObject() {}
};

TEST(NativeType, SkipDestructorNonTrivial) {
  EXPECT_FALSE(NativeType::SkipDestructor(NonTrivialObject{}));
}

struct SkippableDestructObject {
  // Not "= default" on purpose to make this non-trivial.
  // NOLINTNEXTLINE(modernize-use-equals-default)
  ~SkippableDestructObject() {}
};

}  // namespace

template <>
struct NativeTypeTraits<SkippableDestructObject> final {
  static bool SkipDestructor(const SkippableDestructObject&) { return true; }
};

namespace {

TEST(NativeType, SkipDestructorTraits) {
  EXPECT_TRUE(NativeType::SkipDestructor(SkippableDestructObject{}));
}

}  // namespace

}  // namespace cel
