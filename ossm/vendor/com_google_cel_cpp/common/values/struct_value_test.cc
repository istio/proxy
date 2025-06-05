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

#include "absl/base/attributes.h"
#include "common/value.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::cel::internal::DynamicParseTextProto;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::testing::An;
using ::testing::Optional;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

TEST(StructValue, Is) {
  EXPECT_TRUE(StructValue(ParsedMessageValue()).Is<MessageValue>());
  EXPECT_TRUE(StructValue(ParsedMessageValue()).Is<ParsedMessageValue>());
}

template <typename T>
constexpr T& AsLValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return t;
}

template <typename T>
constexpr const T& AsConstLValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return t;
}

template <typename T>
constexpr T&& AsRValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return static_cast<T&&>(t);
}

template <typename T>
constexpr const T&& AsConstRValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return static_cast<const T&&>(t);
}

TEST(StructValue, As) {
  google::protobuf::Arena arena;

  {
    StructValue value(
        ParsedMessageValue{DynamicParseTextProto<TestAllTypesProto3>(
            &arena, R"pb()pb", GetTestingDescriptorPool(),
            GetTestingMessageFactory())});
    StructValue other_value = value;
    EXPECT_THAT(AsLValueRef<StructValue>(value).As<MessageValue>(),
                Optional(An<MessageValue>()));
    EXPECT_THAT(AsConstLValueRef<StructValue>(value).As<MessageValue>(),
                Optional(An<MessageValue>()));
    EXPECT_THAT(AsRValueRef<StructValue>(value).As<MessageValue>(),
                Optional(An<MessageValue>()));
    EXPECT_THAT(AsConstRValueRef<StructValue>(other_value).As<MessageValue>(),
                Optional(An<MessageValue>()));
  }

  {
    StructValue value(
        ParsedMessageValue{DynamicParseTextProto<TestAllTypesProto3>(
            &arena, R"pb()pb", GetTestingDescriptorPool(),
            GetTestingMessageFactory())});
    StructValue other_value = value;
    EXPECT_THAT(AsLValueRef<StructValue>(value).As<ParsedMessageValue>(),
                Optional(An<ParsedMessageValue>()));
    EXPECT_THAT(AsConstLValueRef<StructValue>(value).As<ParsedMessageValue>(),
                Optional(An<ParsedMessageValue>()));
    EXPECT_THAT(AsRValueRef<StructValue>(value).As<ParsedMessageValue>(),
                Optional(An<ParsedMessageValue>()));
    EXPECT_THAT(
        AsConstRValueRef<StructValue>(other_value).As<ParsedMessageValue>(),
        Optional(An<ParsedMessageValue>()));
  }
}

template <typename To, typename From>
decltype(auto) DoGet(From&& from) {
  return std::forward<From>(from).template Get<To>();
}

TEST(StructValue, Get) {
  google::protobuf::Arena arena;

  {
    StructValue value(
        ParsedMessageValue{DynamicParseTextProto<TestAllTypesProto3>(
            &arena, R"pb()pb", GetTestingDescriptorPool(),
            GetTestingMessageFactory())});
    StructValue other_value = value;
    EXPECT_THAT(DoGet<MessageValue>(AsLValueRef<StructValue>(value)),
                An<MessageValue>());
    EXPECT_THAT(DoGet<MessageValue>(AsConstLValueRef<StructValue>(value)),
                An<MessageValue>());
    EXPECT_THAT(DoGet<MessageValue>(AsRValueRef<StructValue>(value)),
                An<MessageValue>());
    EXPECT_THAT(DoGet<MessageValue>(AsConstRValueRef<StructValue>(other_value)),
                An<MessageValue>());
  }

  {
    StructValue value(
        ParsedMessageValue{DynamicParseTextProto<TestAllTypesProto3>(
            &arena, R"pb()pb", GetTestingDescriptorPool(),
            GetTestingMessageFactory())});
    StructValue other_value = value;
    EXPECT_THAT(DoGet<ParsedMessageValue>(AsLValueRef<StructValue>(value)),
                An<ParsedMessageValue>());
    EXPECT_THAT(DoGet<ParsedMessageValue>(AsConstLValueRef<StructValue>(value)),
                An<ParsedMessageValue>());
    EXPECT_THAT(DoGet<ParsedMessageValue>(AsRValueRef<StructValue>(value)),
                An<ParsedMessageValue>());
    EXPECT_THAT(
        DoGet<ParsedMessageValue>(AsConstRValueRef<StructValue>(other_value)),
        An<ParsedMessageValue>());
  }
}

}  // namespace
}  // namespace cel
