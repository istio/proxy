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

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::cel::internal::DynamicParseTextProto;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::testing::An;
using ::testing::Optional;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class MessageValueTest : public TestWithParam<AllocatorKind> {
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

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(MessageValueTest, Default) {
  MessageValue value;
  EXPECT_FALSE(value);
  absl::Cord serialized;
  EXPECT_THAT(value.SerializeTo(value_manager(), serialized),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.ConvertToJson(value_manager()),
              StatusIs(absl::StatusCode::kInternal));
  Value scratch;
  EXPECT_THAT(value.Equal(value_manager(), NullValue()),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.Equal(value_manager(), NullValue(), scratch),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.GetFieldByName(value_manager(), ""),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.GetFieldByName(value_manager(), "", scratch),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.GetFieldByNumber(value_manager(), 0),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.GetFieldByNumber(value_manager(), 0, scratch),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.HasFieldByName(""), StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.HasFieldByNumber(0), StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.ForEachField(value_manager(),
                                 [](absl::string_view, const Value&)
                                     -> absl::StatusOr<bool> { return true; }),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.Qualify(value_manager(), {}, false),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.Qualify(value_manager(), {}, false, scratch),
              StatusIs(absl::StatusCode::kInternal));
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

TEST_P(MessageValueTest, Parsed) {
  MessageValue value(
      ParsedMessageValue(DynamicParseTextProto<TestAllTypesProto3>(
          allocator(), R"pb()pb", descriptor_pool(), message_factory())));
  MessageValue other_value = value;
  EXPECT_TRUE(value);
  EXPECT_TRUE(value.Is<ParsedMessageValue>());
  EXPECT_THAT(value.As<ParsedMessageValue>(),
              Optional(An<ParsedMessageValue>()));
  EXPECT_THAT(AsLValueRef<MessageValue>(value).Get<ParsedMessageValue>(),
              An<ParsedMessageValue>());
  EXPECT_THAT(AsConstLValueRef<MessageValue>(value).Get<ParsedMessageValue>(),
              An<ParsedMessageValue>());
  EXPECT_THAT(AsRValueRef<MessageValue>(value).Get<ParsedMessageValue>(),
              An<ParsedMessageValue>());
  EXPECT_THAT(
      AsConstRValueRef<MessageValue>(other_value).Get<ParsedMessageValue>(),
      An<ParsedMessageValue>());
}

TEST_P(MessageValueTest, Kind) {
  MessageValue value;
  EXPECT_EQ(value.kind(), ParsedMessageValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kStruct);
}

TEST_P(MessageValueTest, GetTypeName) {
  MessageValue value(
      ParsedMessageValue(DynamicParseTextProto<TestAllTypesProto3>(
          allocator(), R"pb()pb", descriptor_pool(), message_factory())));
  EXPECT_EQ(value.GetTypeName(), "google.api.expr.test.v1.proto3.TestAllTypes");
}

TEST_P(MessageValueTest, GetRuntimeType) {
  MessageValue value(
      ParsedMessageValue(DynamicParseTextProto<TestAllTypesProto3>(
          allocator(), R"pb()pb", descriptor_pool(), message_factory())));
  EXPECT_EQ(value.GetRuntimeType(), MessageType(value.GetDescriptor()));
}

INSTANTIATE_TEST_SUITE_P(MessageValueTest, MessageValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
