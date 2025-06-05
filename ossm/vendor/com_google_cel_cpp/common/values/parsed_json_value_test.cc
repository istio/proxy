// Copyright 2024 Google LLC
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

#include "common/values/parsed_json_value.h"

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/value_testing.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::common_internal {
namespace {

using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::cel::test::BoolValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::ListValueElements;
using ::cel::test::ListValueIs;
using ::cel::test::MapValueElements;
using ::cel::test::MapValueIs;
using ::cel::test::StringValueIs;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedJsonValueTest : public TestWithParam<AllocatorKind> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case AllocatorKind::kArena:
        arena_.emplace();
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::Pooling(arena()),
            NewThreadCompatibleTypeReflector(MemoryManager::Pooling(arena())));
        break;
      case AllocatorKind::kNewDelete:
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::ReferenceCounting(),
            NewThreadCompatibleTypeReflector(
                MemoryManager::ReferenceCounting()));
        break;
    }
  }

  void TearDown() override {
    value_manager_.reset();
    arena_.reset();
  }

  Allocator<> allocator() {
    return arena_ ? Allocator(ArenaAllocator<>{&*arena_})
                  : Allocator(NewDeleteAllocator<>{});
  }

  absl::Nullable<google::protobuf::Arena*> arena() { return allocator().arena(); }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  ValueManager& value_manager() { return **value_manager_; }

  template <typename T>
  auto GeneratedParseTextProto(absl::string_view text) {
    return ::cel::internal::GeneratedParseTextProto<T>(
        allocator(), text, descriptor_pool(), message_factory());
  }

  template <typename T>
  auto DynamicParseTextProto(absl::string_view text) {
    return ::cel::internal::DynamicParseTextProto<T>(
        allocator(), text, descriptor_pool(), message_factory());
  }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(ParsedJsonValueTest, Null_Dynamic) {
  EXPECT_THAT(
      ParsedJsonValue(arena(), DynamicParseTextProto<google::protobuf::Value>(
                                   R"pb(null_value: NULL_VALUE)pb")),
      IsNullValue());
  EXPECT_THAT(
      ParsedJsonValue(arena(), DynamicParseTextProto<google::protobuf::Value>(
                                   R"pb(null_value: NULL_VALUE)pb")),
      IsNullValue());
}

TEST_P(ParsedJsonValueTest, Bool_Dynamic) {
  EXPECT_THAT(
      ParsedJsonValue(arena(), DynamicParseTextProto<google::protobuf::Value>(
                                   R"pb(bool_value: true)pb")),
      BoolValueIs(true));
}

TEST_P(ParsedJsonValueTest, Double_Dynamic) {
  EXPECT_THAT(
      ParsedJsonValue(arena(), DynamicParseTextProto<google::protobuf::Value>(
                                   R"pb(number_value: 1.0)pb")),
      DoubleValueIs(1.0));
}

TEST_P(ParsedJsonValueTest, String_Dynamic) {
  EXPECT_THAT(
      ParsedJsonValue(arena(), DynamicParseTextProto<google::protobuf::Value>(
                                   R"pb(string_value: "foo")pb")),
      StringValueIs("foo"));
}

TEST_P(ParsedJsonValueTest, List_Dynamic) {
  EXPECT_THAT(
      ParsedJsonValue(arena(), DynamicParseTextProto<google::protobuf::Value>(
                                   R"pb(list_value: {
                                          values {}
                                          values { bool_value: true }
                                        })pb")),
      ListValueIs(ListValueElements(
          &value_manager(), ElementsAre(IsNullValue(), BoolValueIs(true)))));
}

TEST_P(ParsedJsonValueTest, Map_Dynamic) {
  EXPECT_THAT(
      ParsedJsonValue(arena(), DynamicParseTextProto<google::protobuf::Value>(
                                   R"pb(struct_value: {
                                          fields {
                                            key: "foo"
                                            value: {}
                                          }
                                          fields {
                                            key: "bar"
                                            value: { bool_value: true }
                                          }
                                        })pb")),
      MapValueIs(MapValueElements(
          &value_manager(),
          UnorderedElementsAre(
              Pair(StringValueIs("foo"), IsNullValue()),
              Pair(StringValueIs("bar"), BoolValueIs(true))))));
}

INSTANTIATE_TEST_SUITE_P(ParsedJsonValueTest, ParsedJsonValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel::common_internal
