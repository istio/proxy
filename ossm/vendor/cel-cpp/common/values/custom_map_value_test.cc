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
#include "common/values/list_value_builder.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
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
using ::cel::test::StringValueIs;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class CustomMapValueTest;

struct CustomMapValueTestContent {
  google::protobuf::Arena* absl_nonnull arena;
};

class CustomMapValueInterfaceTest final : public CustomMapValueInterface {
 public:
  std::string DebugString() const override {
    return "{\"foo\": true, \"bar\": 1}";
  }

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

  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    google::protobuf::Struct json_object;
    (*json_object.mutable_fields())["foo"].set_bool_value(true);
    (*json_object.mutable_fields())["bar"].set_number_value(1.0);
    absl::Cord serialized;
    if (!json_object.SerializePartialToString(&serialized)) {
      return absl::UnknownError("failed to serialize google.protobuf.Struct");
    }
    if (!json->ParsePartialFromString(serialized)) {
      return absl::UnknownError("failed to parse google.protobuf.Struct");
    }
    return absl::OkStatus();
  }

  size_t Size() const override { return 2; }

  absl::Status ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      ListValue* absl_nonnull result) const override {
    auto builder = common_internal::NewListValueBuilder(arena);
    builder->Reserve(2);
    CEL_RETURN_IF_ERROR(builder->Add(StringValue("foo")));
    CEL_RETURN_IF_ERROR(builder->Add(StringValue("bar")));
    *result = std::move(*builder).Build();
    return absl::OkStatus();
  }

  CustomMapValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    return CustomMapValue(
        (::new (arena->AllocateAligned(sizeof(CustomMapValueInterfaceTest),
                                       alignof(CustomMapValueInterfaceTest)))
             CustomMapValueInterfaceTest()),
        arena);
  }

 private:
  absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull result) const override {
    if (auto string_key = key.AsString(); string_key) {
      if (*string_key == "foo") {
        *result = TrueValue();
        return true;
      }
      if (*string_key == "bar") {
        *result = IntValue(1);
        return true;
      }
    }
    return false;
  }

  absl::StatusOr<bool> Has(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    if (auto string_key = key.AsString(); string_key) {
      if (*string_key == "foo") {
        return true;
      }
      if (*string_key == "bar") {
        return true;
      }
    }
    return false;
  }

  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<CustomMapValueInterfaceTest>();
  }
};

class CustomMapValueTest : public common_internal::ValueTest<> {
 public:
  CustomMapValue MakeInterface() {
    return CustomMapValue(
        (::new (arena()->AllocateAligned(sizeof(CustomMapValueInterfaceTest),
                                         alignof(CustomMapValueInterfaceTest)))
             CustomMapValueInterfaceTest()),
        arena());
  }

  CustomMapValue MakeDispatcher() {
    return UnsafeCustomMapValue(
        &test_dispatcher_, CustomValueContent::From<CustomMapValueTestContent>(
                               CustomMapValueTestContent{.arena = arena()}));
  }

 protected:
  CustomMapValueDispatcher test_dispatcher_ = {
      .get_type_id = [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
                        CustomMapValueContent content) -> NativeTypeId {
        return NativeTypeId::For<CustomMapValueTest>();
      },
      .get_arena =
          [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
             CustomMapValueContent content) -> google::protobuf::Arena* absl_nullable {
        return content.To<CustomMapValueTestContent>().arena;
      },
      .debug_string =
          [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
             CustomMapValueContent content) -> std::string {
        return "{\"foo\": true, \"bar\": 1}";
      },
      .serialize_to =
          [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
             CustomMapValueContent content,
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
      .convert_to_json_object =
          [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
             CustomMapValueContent content,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Message* absl_nonnull json) -> absl::Status {
        {
          google::protobuf::Struct json_object;
          (*json_object.mutable_fields())["foo"].set_bool_value(true);
          (*json_object.mutable_fields())["bar"].set_number_value(1.0);
          absl::Cord serialized;
          if (!json_object.SerializePartialToString(&serialized)) {
            return absl::UnknownError(
                "failed to serialize google.protobuf.Struct");
          }
          if (!json->ParsePartialFromString(serialized)) {
            return absl::UnknownError("failed to parse google.protobuf.Struct");
          }
          return absl::OkStatus();
        }
      },
      .is_zero_value =
          [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
             CustomMapValueContent content) -> bool { return false; },
      .size = [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
                 CustomMapValueContent content) -> size_t { return 2; },
      .find = [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
                 CustomMapValueContent content, const Value& key,
                 const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                 google::protobuf::MessageFactory* absl_nonnull message_factory,
                 google::protobuf::Arena* absl_nonnull arena,
                 Value* absl_nonnull result) -> absl::StatusOr<bool> {
        if (auto string_key = key.AsString(); string_key) {
          if (*string_key == "foo") {
            *result = TrueValue();
            return true;
          }
          if (*string_key == "bar") {
            *result = IntValue(1);
            return true;
          }
        }
        return false;
      },
      .has = [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
                CustomMapValueContent content, const Value& key,
                const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                google::protobuf::MessageFactory* absl_nonnull message_factory,
                google::protobuf::Arena* absl_nonnull arena) -> absl::StatusOr<bool> {
        if (auto string_key = key.AsString(); string_key) {
          if (*string_key == "foo") {
            return true;
          }
          if (*string_key == "bar") {
            return true;
          }
        }
        return false;
      },
      .list_keys =
          [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
             CustomMapValueContent content,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Arena* absl_nonnull arena,
             ListValue* absl_nonnull result) -> absl::Status {
        auto builder = common_internal::NewListValueBuilder(arena);
        builder->Reserve(2);
        CEL_RETURN_IF_ERROR(builder->Add(StringValue("foo")));
        CEL_RETURN_IF_ERROR(builder->Add(StringValue("bar")));
        *result = std::move(*builder).Build();
        return absl::OkStatus();
      },
      .clone = [](const CustomMapValueDispatcher* absl_nonnull dispatcher,
                  CustomMapValueContent content,
                  google::protobuf::Arena* absl_nonnull arena) -> CustomMapValue {
        return UnsafeCustomMapValue(
            dispatcher, CustomValueContent::From<CustomMapValueTestContent>(
                            CustomMapValueTestContent{.arena = arena}));
      },
  };
};

TEST_F(CustomMapValueTest, Kind) {
  EXPECT_EQ(CustomMapValue::kind(), CustomMapValue::kKind);
}

TEST_F(CustomMapValueTest, Dispatcher_GetTypeId) {
  EXPECT_EQ(MakeDispatcher().GetTypeId(),
            NativeTypeId::For<CustomMapValueTest>());
}

TEST_F(CustomMapValueTest, Interface_GetTypeId) {
  EXPECT_EQ(MakeInterface().GetTypeId(),
            NativeTypeId::For<CustomMapValueInterfaceTest>());
}

TEST_F(CustomMapValueTest, Dispatcher_GetTypeName) {
  EXPECT_EQ(MakeDispatcher().GetTypeName(), "map");
}

TEST_F(CustomMapValueTest, Interface_GetTypeName) {
  EXPECT_EQ(MakeInterface().GetTypeName(), "map");
}

TEST_F(CustomMapValueTest, Dispatcher_DebugString) {
  EXPECT_EQ(MakeDispatcher().DebugString(), "{\"foo\": true, \"bar\": 1}");
}

TEST_F(CustomMapValueTest, Interface_DebugString) {
  EXPECT_EQ(MakeInterface().DebugString(), "{\"foo\": true, \"bar\": 1}");
}

TEST_F(CustomMapValueTest, Dispatcher_IsZeroValue) {
  EXPECT_FALSE(MakeDispatcher().IsZeroValue());
}

TEST_F(CustomMapValueTest, Interface_IsZeroValue) {
  EXPECT_FALSE(MakeInterface().IsZeroValue());
}

TEST_F(CustomMapValueTest, Dispatcher_SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(MakeDispatcher().SerializeTo(descriptor_pool(), message_factory(),
                                           &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), Not(IsEmpty()));
}

TEST_F(CustomMapValueTest, Interface_SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(MakeInterface().SerializeTo(descriptor_pool(), message_factory(),
                                          &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), Not(IsEmpty()));
}

TEST_F(CustomMapValueTest, Dispatcher_ConvertToJson) {
  auto message = DynamicParseTextProto<google::protobuf::Value>();
  EXPECT_THAT(
      MakeDispatcher().ConvertToJson(descriptor_pool(), message_factory(),
                                     cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::Value>(R"pb(
    struct_value: {
      fields: {
        key: "foo"
        value: { bool_value: true }
      }
      fields: {
        key: "bar"
        value: { number_value: 1.0 }
      }
    }
  )pb"));
}

TEST_F(CustomMapValueTest, Interface_ConvertToJson) {
  auto message = DynamicParseTextProto<google::protobuf::Value>();
  EXPECT_THAT(
      MakeInterface().ConvertToJson(descriptor_pool(), message_factory(),
                                    cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::Value>(R"pb(
    struct_value: {
      fields: {
        key: "foo"
        value: { bool_value: true }
      }
      fields: {
        key: "bar"
        value: { number_value: 1.0 }
      }
    }
  )pb"));
}

TEST_F(CustomMapValueTest, Dispatcher_ConvertToJsonObject) {
  auto message = DynamicParseTextProto<google::protobuf::Struct>();
  EXPECT_THAT(
      MakeDispatcher().ConvertToJsonObject(descriptor_pool(), message_factory(),
                                           cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::Struct>(R"pb(
    fields: {
      key: "foo"
      value: { bool_value: true }
    }
    fields: {
      key: "bar"
      value: { number_value: 1.0 }
    }
  )pb"));
}

TEST_F(CustomMapValueTest, Interface_ConvertToJsonObject) {
  auto message = DynamicParseTextProto<google::protobuf::Struct>();
  EXPECT_THAT(
      MakeInterface().ConvertToJsonObject(descriptor_pool(), message_factory(),
                                          cel::to_address(message)),
      IsOk());
  EXPECT_THAT(*message, EqualsTextProto<google::protobuf::Struct>(R"pb(
    fields: {
      key: "foo"
      value: { bool_value: true }
    }
    fields: {
      key: "bar"
      value: { number_value: 1.0 }
    }
  )pb"));
}

TEST_F(CustomMapValueTest, Dispatcher_IsEmpty) {
  EXPECT_FALSE(MakeDispatcher().IsEmpty());
}

TEST_F(CustomMapValueTest, Interface_IsEmpty) {
  EXPECT_FALSE(MakeInterface().IsEmpty());
}

TEST_F(CustomMapValueTest, Dispatcher_Size) {
  EXPECT_EQ(MakeDispatcher().Size(), 2);
}

TEST_F(CustomMapValueTest, Interface_Size) {
  EXPECT_EQ(MakeInterface().Size(), 2);
}

TEST_F(CustomMapValueTest, Dispatcher_Get) {
  CustomMapValue map = MakeDispatcher();
  ASSERT_THAT(map.Get(StringValue("foo"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(map.Get(StringValue("bar"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(IntValueIs(1)));
  ASSERT_THAT(
      map.Get(StringValue("baz"), descriptor_pool(), message_factory(),
              arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_F(CustomMapValueTest, Interface_Get) {
  CustomMapValue map = MakeInterface();
  ASSERT_THAT(map.Get(StringValue("foo"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(map.Get(StringValue("bar"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(IntValueIs(1)));
  ASSERT_THAT(
      map.Get(StringValue("baz"), descriptor_pool(), message_factory(),
              arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_F(CustomMapValueTest, Dispatcher_Find) {
  CustomMapValue map = MakeDispatcher();
  ASSERT_THAT(map.Find(StringValue("foo"), descriptor_pool(), message_factory(),
                       arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  ASSERT_THAT(map.Find(StringValue("bar"), descriptor_pool(), message_factory(),
                       arena()),
              IsOkAndHolds(Optional(IntValueIs(1))));
  ASSERT_THAT(map.Find(StringValue("baz"), descriptor_pool(), message_factory(),
                       arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomMapValueTest, Interface_Find) {
  CustomMapValue map = MakeInterface();
  ASSERT_THAT(map.Find(StringValue("foo"), descriptor_pool(), message_factory(),
                       arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  ASSERT_THAT(map.Find(StringValue("bar"), descriptor_pool(), message_factory(),
                       arena()),
              IsOkAndHolds(Optional(IntValueIs(1))));
  ASSERT_THAT(map.Find(StringValue("baz"), descriptor_pool(), message_factory(),
                       arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomMapValueTest, Dispatcher_Has) {
  CustomMapValue map = MakeDispatcher();
  ASSERT_THAT(map.Has(StringValue("foo"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(map.Has(StringValue("bar"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(map.Has(StringValue("baz"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_F(CustomMapValueTest, Interface_Has) {
  CustomMapValue map = MakeInterface();
  ASSERT_THAT(map.Has(StringValue("foo"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(map.Has(StringValue("bar"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_THAT(map.Has(StringValue("baz"), descriptor_pool(), message_factory(),
                      arena()),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_F(CustomMapValueTest, Dispatcher_ForEach) {
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      MakeDispatcher().ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{key, value});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), BoolValueIs(true)),
                           Pair(StringValueIs("bar"), IntValueIs(1))));
}

TEST_F(CustomMapValueTest, Interface_ForEach) {
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      MakeInterface().ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{key, value});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), BoolValueIs(true)),
                           Pair(StringValueIs("bar"), IntValueIs(1))));
}

TEST_F(CustomMapValueTest, Dispatcher_NewIterator) {
  CustomMapValue map = MakeDispatcher();
  ASSERT_OK_AND_ASSIGN(auto iterator, map.NewIterator());
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(StringValueIs("foo")));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(StringValueIs("bar")));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CustomMapValueTest, Interface_NewIterator) {
  CustomMapValue map = MakeInterface();
  ASSERT_OK_AND_ASSIGN(auto iterator, map.NewIterator());
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(StringValueIs("foo")));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(StringValueIs("bar")));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CustomMapValueTest, Dispatcher_NewIterator1) {
  CustomMapValue map = MakeDispatcher();
  ASSERT_OK_AND_ASSIGN(auto iterator, map.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(StringValueIs("foo"))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(StringValueIs("bar"))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomMapValueTest, Interface_NewIterator1) {
  CustomMapValue map = MakeInterface();
  ASSERT_OK_AND_ASSIGN(auto iterator, map.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(StringValueIs("foo"))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(StringValueIs("bar"))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomMapValueTest, Dispatcher_NewIterator2) {
  CustomMapValue map = MakeDispatcher();
  ASSERT_OK_AND_ASSIGN(auto iterator, map.NewIterator());
  EXPECT_THAT(
      iterator->Next2(descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(Optional(Pair(StringValueIs("foo"), BoolValueIs(true)))));
  EXPECT_THAT(
      iterator->Next2(descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(Optional(Pair(StringValueIs("bar"), IntValueIs(1)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomMapValueTest, Interface_NewIterator2) {
  CustomMapValue map = MakeInterface();
  ASSERT_OK_AND_ASSIGN(auto iterator, map.NewIterator());
  EXPECT_THAT(
      iterator->Next2(descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(Optional(Pair(StringValueIs("foo"), BoolValueIs(true)))));
  EXPECT_THAT(
      iterator->Next2(descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(Optional(Pair(StringValueIs("bar"), IntValueIs(1)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(CustomMapValueTest, Dispatcher) {
  EXPECT_THAT(MakeDispatcher().dispatcher(), NotNull());
  EXPECT_THAT(MakeDispatcher().interface(), IsNull());
}

TEST_F(CustomMapValueTest, Interface) {
  EXPECT_THAT(MakeInterface().dispatcher(), IsNull());
  EXPECT_THAT(MakeInterface().interface(), NotNull());
}

}  // namespace
}  // namespace cel
