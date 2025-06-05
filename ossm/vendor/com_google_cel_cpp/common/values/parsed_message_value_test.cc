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

#include "absl/base/nullability.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_kind.h"
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

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::cel::internal::DynamicParseTextProto;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::cel::test::BoolValueIs;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;
using ::testing::VariantWith;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedMessageValueTest : public TestWithParam<AllocatorKind> {
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
  ParsedMessageValue MakeParsedMessage(absl::string_view text) {
    return ParsedMessageValue(DynamicParseTextProto<T>(
        allocator(), R"pb()pb", descriptor_pool(), message_factory()));
  }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(ParsedMessageValueTest, Default) {
  ParsedMessageValue value;
  EXPECT_FALSE(value);
}

TEST_P(ParsedMessageValueTest, Field) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_TRUE(value);
}

TEST_P(ParsedMessageValueTest, Kind) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_EQ(value.kind(), ParsedMessageValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kStruct);
}

TEST_P(ParsedMessageValueTest, GetTypeName) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_EQ(value.GetTypeName(), "google.api.expr.test.v1.proto3.TestAllTypes");
}

TEST_P(ParsedMessageValueTest, GetRuntimeType) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_EQ(value.GetRuntimeType(), MessageType(value.GetDescriptor()));
}

TEST_P(ParsedMessageValueTest, DebugString) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_THAT(value.DebugString(), _);
}

TEST_P(ParsedMessageValueTest, IsZeroValue) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_TRUE(value.IsZeroValue());
}

TEST_P(ParsedMessageValueTest, SerializeTo) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  absl::Cord serialized;
  EXPECT_THAT(value.SerializeTo(value_manager(), serialized), IsOk());
  EXPECT_THAT(serialized, IsEmpty());
}

TEST_P(ParsedMessageValueTest, ConvertToJson) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_THAT(value.ConvertToJson(value_manager()),
              IsOkAndHolds(VariantWith<JsonObject>(JsonObject())));
}

TEST_P(ParsedMessageValueTest, Equal) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_THAT(value.Equal(value_manager(), BoolValue()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Equal(value_manager(),
                          MakeParsedMessage<TestAllTypesProto3>(R"pb()pb")),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ParsedMessageValueTest, GetFieldByName) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_THAT(value.GetFieldByName(value_manager(), "single_bool"),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_P(ParsedMessageValueTest, GetFieldByNumber) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>(R"pb()pb");
  EXPECT_THAT(value.GetFieldByNumber(value_manager(), 13),
              IsOkAndHolds(BoolValueIs(false)));
}

INSTANTIATE_TEST_SUITE_P(ParsedMessageValueTest, ParsedMessageValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
