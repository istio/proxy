// Copyright 2025 Google LLC
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

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::IntValueIs;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class CustomListValueTest;

struct CustomListValueTestContent {
  google::protobuf::Arena* absl_nonnull arena;
};

class CustomListValueInterfaceTest final : public CustomListValueInterface {
 public:
  std::string DebugString() const override { return "[true, 1]"; }

  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const override {
    google::protobuf::Value json;
    google::protobuf::ListValue* json_array = json.mutable_list_value();
    json_array->add_values()->set_bool_value(true);
    json_array->add_values()->set_number_value(1.0);
    if (!json.SerializePartialToZeroCopyStream(output)) {
      return absl::UnknownError(
          "failed to serialize message: google.protobuf.Value");
    }
    return absl::OkStatus();
  }

  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    google::protobuf::ListValue json_array;
    json_array.add_values()->set_bool_value(true);
    json_array.add_values()->set_number_value(1.0);
    absl::Cord serialized;
    if (!json_array.SerializePartialToString(&serialized)) {
      return absl::UnknownError(
          "failed to serialize google.protobuf.ListValue");
    }
    if (!json->ParsePartialFromString(serialized)) {
      return absl::UnknownError("failed to parse google.protobuf.ListValue");
    }
    return absl::OkStatus();
  }

  size_t Size() const override { return 2; }

  CustomListValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    return CustomListValue(
        (::new (arena->AllocateAligned(sizeof(CustomListValueInterfaceTest),
                                       alignof(CustomListValueInterfaceTest)))
             CustomListValueInterfaceTest()),
        arena);
  }

 private:
  absl::Status Get(size_t index,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const override {
    if (index == 0) {
      *result = TrueValue();
      return absl::OkStatus();
    }
    if (index == 1) {
      *result = IntValue(1);
      return absl::OkStatus();
    }
    *result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }

  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<CustomListValueInterfaceTest>();
  }
};

class CustomListValueTest : public common_internal::ValueTest<> {
 public:
  CustomListValue MakeInterface() {
    return CustomListValue(
        (::new (arena()->AllocateAligned(sizeof(CustomListValueInterfaceTest),
                                         alignof(CustomListValueInterfaceTest)))
             CustomListValueInterfaceTest()),
        arena());
  }

  CustomListValue MakeDispatcher() {
    return UnsafeCustomListValue(
        &test_dispatcher_, CustomValueContent::From<CustomListValueTestContent>(
                               CustomListValueTestContent{.arena = arena()}));
  }

 protected:
  CustomListValueDispatcher test_dispatcher_ = {
      .get_type_id =
          [](const CustomListValueDispatcher* absl_nonnull dispatcher,
             CustomListValueContent content) -> NativeTypeId {
        return NativeTypeId::For<CustomListValueTest>();
      },
      .get_arena =
          [](const CustomListValueDispatcher* absl_nonnull dispatcher,
             CustomListValueContent content) -> google::protobuf::Arena* absl_nullable {
        return content.To<CustomListValueTestContent>().arena;
      },
      .debug_string =
          [](const CustomListValueDispatcher* absl_nonnull dispatcher,
             CustomListValueContent content) -> std::string {
        return "[true, 1]";
      },
      .serialize_to =
          [](const CustomListValueDispatcher* absl_nonnull dispatcher,
             CustomListValueContent content,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output)
          -> absl::Status {
        google::protobuf::Value json;
        google::protobuf::Struct* json_object = json.mutable_struct_value();
        (*json_object->mutable_fields())["foo"].set_bool_value(true);
        (*json_object->mutable_fields())["bar"].set_number_value(1.0);
        if (!json.SerializePartialToZeroCopyStream(output)) {
          return absl::UnknownError(
              "failed to serialize message: google.protobuf.Value");
        }
        return absl::OkStatus();
      },
      .convert_to_json_array =
          [](const CustomListValueDispatcher* absl_nonnull dispatcher,
             CustomListValueContent content,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Message* absl_nonnull json) -> absl::Status {
        {
          google::protobuf::ListValue json_array;
          json_array.add_values()->set_bool_value(true);
          json_array.add_values()->set_number_value(1.0);
          absl::Cord serialized;
          if (!json_array.SerializePartialToString(&serialized)) {
            return absl::UnknownError(
                "failed to serialize google.protobuf.ListValue");
          }
          if (!json->ParsePartialFromString(serialized)) {
            return absl::UnknownError(
                "failed to parse google.protobuf.ListValue");
          }
          return absl::OkStatus();
        }
      },
      .is_zero_value =
          [](const CustomListValueDispatcher* absl_nonnull dispatcher,
             CustomListValueContent content) -> bool { return false; },
      .size = [](const CustomListValueDispatcher* absl_nonnull dispatcher,
                 CustomListValueContent content) -> size_t { return 2; },
      .get = [](const CustomListValueDispatcher* absl_nonnull dispatcher,
                CustomListValueContent content, size_t index,
                const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                google::protobuf::MessageFactory* absl_nonnull message_factory,
                google::protobuf::Arena* absl_nonnull arena,
                Value* absl_nonnull result) -> absl::Status {
        if (index == 0) {
          *result = TrueValue();
          return absl::OkStatus();
        }
        if (index == 1) {
          *result = IntValue(1);
          return absl::OkStatus();
        }
        *result = IndexOutOfBoundsError(index);
        return absl::OkStatus();
      },
      .clone = [](const CustomListValueDispatcher* absl_nonnull dispatcher,
                  CustomListValueContent content,
                  google::protobuf::Arena* absl_nonnull arena) -> CustomListValue {
        return UnsafeCustomListValue(
            dispatcher, CustomValueContent::From<CustomListValueTestContent>(
                            CustomListValueTestContent{.arena = arena}));
      },
  };
};

TEST_F(CustomListValueTest, Kind) {
  EXPECT_EQ(CustomListValue::kind(), CustomListValue::kKind);
}

TEST_F(CustomListValueTest, Dispatcher_GetTypeId) {
  EXPECT_EQ(MakeDispatcher().GetTypeId(),
            NativeTypeId::For<CustomListValueTest>());
}

TEST_F(CustomListValueTest, Interface_GetTypeId) {
  EXPECT_EQ(MakeInterface().GetTypeId(),
            NativeTypeId::For<CustomListValueInterfaceTest>());
}

TEST_F(CustomListValueTest, Dispatcher_GetTypeName) {
  EXPECT_EQ(MakeDispatcher().GetTypeName(), "list");
}

TEST_F(CustomListValueTest, Interface_GetTypeName) {
  EXPECT_EQ(MakeInterface().GetTypeName(), "list");
}

TEST_F(CustomListValueTest, Dispatcher_DebugString) {
  EXPECT_EQ(MakeDispatcher().DebugString(), "[true, 1]");
}

TEST_F(CustomListValueTest, Interface_DebugString) {
  EXPECT_EQ(MakeInterface().DebugString(), "[true, 1]");
}

TEST_F(CustomListValueTest, Dispatcher_IsZeroValue) {
  EXPECT_FALSE(MakeDispatcher().IsZeroValue());
}

TEST_F(CustomListValueTest, Interface_IsZeroValue) {
  EXPECT_FALSE(MakeInterface().IsZeroValue());
}

TEST_F(CustomListValueTest, Dispatcher_SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(MakeDispatcher().SerializeTo(descriptor_pool(), message_factory(),
                                           &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), Not(IsEmpty()));
}

TEST_F(CustomListValueTest, Interface_SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(MakeInterface().SerializeTo(descriptor_pool(), message_factory(),
                                          &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), Not(IsEmpty()));
}

TEST_F(CustomListValueTest, Dispatcher_ConvertToJson) {
  auto message = DynamicParseTextProto<google::protobuf::Value>();
  EXPECT_THAT(
      MakeDispatcher().ConvertToJson(descriptor_pool(), message_factory(),
                                     cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::Value>(R"pb(
    list_value: {
      values: { bool_value: true }
      values: { number_value: 1.0 }
    }
  )pb"));
}

TEST_F(CustomListValueTest, Interface_ConvertToJson) {
  auto message = DynamicParseTextProto<google::protobuf::Value>();
  EXPECT_THAT(
      MakeInterface().ConvertToJson(descriptor_pool(), message_factory(),
                                    cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::Value>(R"pb(
    list_value: {
      values: { bool_value: true }
      values: { number_value: 1.0 }
    }
  )pb"));
}

TEST_F(CustomListValueTest, Dispatcher_ConvertToJsonArray) {
  auto message = DynamicParseTextProto<google::protobuf::ListValue>();
  EXPECT_THAT(
      MakeDispatcher().ConvertToJsonArray(descriptor_pool(), message_factory(),
                                          cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::ListValue>(R"pb(
    values: { bool_value: true }
    values: { number_value: 1.0 }
  )pb"));
}

TEST_F(CustomListValueTest, Interface_ConvertToJsonArray) {
  auto message = DynamicParseTextProto<google::protobuf::ListValue>();
  EXPECT_THAT(
      MakeInterface().ConvertToJsonArray(descriptor_pool(), message_factory(),
                                         cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::ListValue>(R"pb(
    values: { bool_value: true }
    values: { number_value: 1.0 }
  )pb"));
}

TEST_F(CustomListValueTest, Dispatcher_IsEmpty) {
  EXPECT_FALSE(MakeDispatcher().IsEmpty());
}

TEST_F(CustomListValueTest, Interface_IsEmpty) {
  EXPECT_FALSE(MakeInterface().IsEmpty());
}

TEST_F(CustomListValueTest, Dispatcher_Size) {
  EXPECT_EQ(MakeDispatcher().Size(), 2);
}

TEST_F(CustomListValueTest, Interface_Size) {
  EXPECT_EQ(MakeInterface().Size(), 2);
}

TEST_F(CustomListValueTest, Dispatcher_Get) {
  CustomListValue list = MakeDispatcher();
  ASSERT_THAT(list.Get(0, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(list.Get(1, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
  ASSERT_THAT(
      list.Get(2, descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_F(CustomListValueTest, Interface_Get) {
  CustomListValue list = MakeInterface();
  ASSERT_THAT(list.Get(0, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(list.Get(1, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
  ASSERT_THAT(
      list.Get(2, descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_F(CustomListValueTest, Dispatcher_ForEach) {
  std::vector<std::pair<size_t, Value>> fields;
  EXPECT_THAT(
      MakeDispatcher().ForEach(
          [&](size_t index, const Value& value) -> absl::StatusOr<bool> {
            fields.push_back(std::pair{index, value});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(fields, UnorderedElementsAre(Pair(0, BoolValueIs(true)),
                                           Pair(1, IntValueIs(1))));
}

TEST_F(CustomListValueTest, Interface_ForEach) {
  std::vector<std::pair<size_t, Value>> fields;
  EXPECT_THAT(
      MakeInterface().ForEach(
          [&](size_t index, const Value& value) -> absl::StatusOr<bool> {
            fields.push_back(std::pair{index, value});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(fields, UnorderedElementsAre(Pair(0, BoolValueIs(true)),
                                           Pair(1, IntValueIs(1))));
}

TEST_F(CustomListValueTest, Dispatcher_NewIterator) {
  CustomListValue list = MakeDispatcher();
  ASSERT_OK_AND_ASSIGN(auto iterator, list.NewIterator());
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CustomListValueTest, Interface_NewIterator) {
  CustomListValue list = MakeInterface();
  ASSERT_OK_AND_ASSIGN(auto iterator, list.NewIterator());
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CustomListValueTest, Dispatcher_NewIterator1) {
  CustomListValue list = MakeDispatcher();
  ASSERT_OK_AND_ASSIGN(auto iterator, list.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(IntValueIs(1))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomListValueTest, Interface_NewIterator1) {
  CustomListValue list = MakeInterface();
  ASSERT_OK_AND_ASSIGN(auto iterator, list.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(IntValueIs(1))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomListValueTest, Dispatcher_NewIterator2) {
  CustomListValue list = MakeDispatcher();
  ASSERT_OK_AND_ASSIGN(auto iterator, list.NewIterator());
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(0), BoolValueIs(true)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(1), IntValueIs(1)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomListValueTest, Interface_NewIterator2) {
  CustomListValue list = MakeInterface();
  ASSERT_OK_AND_ASSIGN(auto iterator, list.NewIterator());
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(0), BoolValueIs(true)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(1), IntValueIs(1)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomListValueTest, Dispatcher_Contains) {
  CustomListValue list = MakeDispatcher();
  EXPECT_THAT(
      list.Contains(TrueValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      list.Contains(IntValue(1), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(list.Contains(UintValue(1u), descriptor_pool(), message_factory(),
                            arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(list.Contains(DoubleValue(1.0), descriptor_pool(),
                            message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(list.Contains(FalseValue(), descriptor_pool(), message_factory(),
                            arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      list.Contains(IntValue(0), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(list.Contains(UintValue(0u), descriptor_pool(), message_factory(),
                            arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(list.Contains(DoubleValue(0.0), descriptor_pool(),
                            message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_F(CustomListValueTest, Interface_Contains) {
  CustomListValue list = MakeInterface();
  EXPECT_THAT(
      list.Contains(TrueValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      list.Contains(IntValue(1), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(list.Contains(UintValue(1u), descriptor_pool(), message_factory(),
                            arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(list.Contains(DoubleValue(1.0), descriptor_pool(),
                            message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(list.Contains(FalseValue(), descriptor_pool(), message_factory(),
                            arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      list.Contains(IntValue(0), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(list.Contains(UintValue(0u), descriptor_pool(), message_factory(),
                            arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(list.Contains(DoubleValue(0.0), descriptor_pool(),
                            message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_F(CustomListValueTest, Dispatcher) {
  EXPECT_THAT(MakeDispatcher().dispatcher(), NotNull());
  EXPECT_THAT(MakeDispatcher().interface(), IsNull());
}

TEST_F(CustomListValueTest, Interface) {
  EXPECT_THAT(MakeInterface().dispatcher(), IsNull());
  EXPECT_THAT(MakeInterface().interface(), NotNull());
}

}  // namespace
}  // namespace cel
