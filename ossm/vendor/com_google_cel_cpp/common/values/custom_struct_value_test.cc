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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/attribute.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/runtime_options.h"
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
using ::cel::test::IntValueIs;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

struct CustomStructValueTest;

struct CustomStructValueTestContent {
  google::protobuf::Arena* absl_nonnull arena;
};

class CustomStructValueInterfaceTest final : public CustomStructValueInterface {
 public:
  absl::string_view GetTypeName() const override { return "test.Interface"; }

  std::string DebugString() const override {
    return std::string(GetTypeName());
  }

  bool IsZeroValue() const override { return false; }

  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const override {
    google::protobuf::Value json;
    google::protobuf::Struct* json_object = json.mutable_struct_value();
    (*json_object->mutable_fields())["foo"].set_bool_value(true);
    (*json_object->mutable_fields())["bar"].set_number_value(1.0);
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
    if (!json_object.SerializePartialToCord(&serialized)) {
      return absl::UnknownError("failed to serialize google.protobuf.Struct");
    }
    if (!json->ParsePartialFromCord(serialized)) {
      return absl::UnknownError("failed to parse google.protobuf.Struct");
    }
    return absl::OkStatus();
  }

  absl::Status GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull result) const override {
    if (name == "foo") {
      *result = TrueValue();
      return absl::OkStatus();
    }
    if (name == "bar") {
      *result = IntValue(1);
      return absl::OkStatus();
    }
    return NoSuchFieldError(name).ToStatus();
  }

  absl::Status GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull result) const override {
    if (number == 1) {
      *result = TrueValue();
      return absl::OkStatus();
    }
    if (number == 2) {
      *result = IntValue(1);
      return absl::OkStatus();
    }
    return NoSuchFieldError(absl::StrCat(number)).ToStatus();
  }

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const override {
    if (name == "foo") {
      return true;
    }
    if (name == "bar") {
      return true;
    }
    return NoSuchFieldError(name).ToStatus();
  }

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const override {
    if (number == 1) {
      return true;
    }
    if (number == 2) {
      return true;
    }
    return NoSuchFieldError(absl::StrCat(number)).ToStatus();
  }

  absl::Status ForEachField(
      ForEachFieldCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    CEL_ASSIGN_OR_RETURN(bool ok, callback("foo", TrueValue()));
    if (!ok) {
      return absl::OkStatus();
    }
    CEL_ASSIGN_OR_RETURN(ok, callback("bar", IntValue(1)));
    return absl::OkStatus();
  }

  CustomStructValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    return CustomStructValue(
        (::new (arena->AllocateAligned(sizeof(CustomStructValueInterfaceTest),
                                       alignof(CustomStructValueInterfaceTest)))
             CustomStructValueInterfaceTest()),
        arena);
  }

 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<CustomStructValueInterfaceTest>();
  }
};

class CustomStructValueTest : public common_internal::ValueTest<> {
 public:
  CustomStructValue MakeInterface() {
    return CustomStructValue((::new (arena()->AllocateAligned(
                                 sizeof(CustomStructValueInterfaceTest),
                                 alignof(CustomStructValueInterfaceTest)))
                                  CustomStructValueInterfaceTest()),
                             arena());
  }

  CustomStructValue MakeDispatcher() {
    return UnsafeCustomStructValue(
        &test_dispatcher_,
        CustomValueContent::From<CustomStructValueTestContent>(
            CustomStructValueTestContent{.arena = arena()}));
  }

 protected:
  CustomStructValueDispatcher test_dispatcher_ = {
      .get_type_id =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content) -> NativeTypeId {
        return NativeTypeId::For<CustomStructValueTest>();
      },
      .get_arena =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content) -> google::protobuf::Arena* absl_nullable {
        return content.To<CustomStructValueTestContent>().arena;
      },
      .get_type_name =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content) -> absl::string_view {
        return "test.Dispatcher";
      },
      .debug_string =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content) -> std::string {
        return "test.Dispatcher";
      },
      .get_runtime_type =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content) -> StructType {
        return common_internal::MakeBasicStructType("test.Dispatcher");
      },
      .serialize_to =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content,
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
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Message* absl_nonnull json) -> absl::Status {
        google::protobuf::Struct json_object;
        (*json_object.mutable_fields())["foo"].set_bool_value(true);
        (*json_object.mutable_fields())["bar"].set_number_value(1.0);
        absl::Cord serialized;
        if (!json_object.SerializePartialToCord(&serialized)) {
          return absl::UnknownError(
              "failed to serialize google.protobuf.Struct");
        }
        if (!json->ParsePartialFromCord(serialized)) {
          return absl::UnknownError("failed to parse google.protobuf.Struct");
        }
        return absl::OkStatus();
      },
      .is_zero_value =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content) -> bool { return false; },
      .get_field_by_name =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content, absl::string_view name,
             ProtoWrapperTypeOptions unboxing_options,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Arena* absl_nonnull arena,
             Value* absl_nonnull result) -> absl::Status {
        if (name == "foo") {
          *result = TrueValue();
          return absl::OkStatus();
        }
        if (name == "bar") {
          *result = IntValue(1);
          return absl::OkStatus();
        }
        return NoSuchFieldError(name).ToStatus();
      },
      .get_field_by_number =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content, int64_t number,
             ProtoWrapperTypeOptions unboxing_options,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Arena* absl_nonnull arena,
             Value* absl_nonnull result) -> absl::Status {
        if (number == 1) {
          *result = TrueValue();
          return absl::OkStatus();
        }
        if (number == 2) {
          *result = IntValue(1);
          return absl::OkStatus();
        }
        return NoSuchFieldError(absl::StrCat(number)).ToStatus();
      },
      .has_field_by_name =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content,
             absl::string_view name) -> absl::StatusOr<bool> {
        if (name == "foo") {
          return true;
        }
        if (name == "bar") {
          return true;
        }
        return NoSuchFieldError(name).ToStatus();
      },
      .has_field_by_number =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content,
             int64_t number) -> absl::StatusOr<bool> {
        if (number == 1) {
          return true;
        }
        if (number == 2) {
          return true;
        }
        return NoSuchFieldError(absl::StrCat(number)).ToStatus();
      },
      .for_each_field =
          [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
             CustomStructValueContent content,
             absl::FunctionRef<absl::StatusOr<bool>(absl::string_view,
                                                    const Value&)>
                 callback,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Arena* absl_nonnull arena) -> absl::Status {
        CEL_ASSIGN_OR_RETURN(bool ok, callback("foo", TrueValue()));
        if (!ok) {
          return absl::OkStatus();
        }
        CEL_ASSIGN_OR_RETURN(ok, callback("bar", IntValue(1)));
        return absl::OkStatus();
      },
      .clone = [](const CustomStructValueDispatcher* absl_nonnull dispatcher,
                  CustomStructValueContent content,
                  google::protobuf::Arena* absl_nonnull arena) -> CustomStructValue {
        return UnsafeCustomStructValue(
            dispatcher, CustomValueContent::From<CustomStructValueTestContent>(
                            CustomStructValueTestContent{.arena = arena}));
      },
  };
};

TEST_F(CustomStructValueTest, Kind) {
  EXPECT_EQ(CustomStructValue::kind(), CustomStructValue::kKind);
}

TEST_F(CustomStructValueTest, Dispatcher_GetTypeId) {
  EXPECT_EQ(MakeDispatcher().GetTypeId(),
            NativeTypeId::For<CustomStructValueTest>());
}

TEST_F(CustomStructValueTest, Interface_GetTypeId) {
  EXPECT_EQ(MakeInterface().GetTypeId(),
            NativeTypeId::For<CustomStructValueInterfaceTest>());
}

TEST_F(CustomStructValueTest, Dispatcher_GetTypeName) {
  EXPECT_EQ(MakeDispatcher().GetTypeName(), "test.Dispatcher");
}

TEST_F(CustomStructValueTest, Interface_GetTypeName) {
  EXPECT_EQ(MakeInterface().GetTypeName(), "test.Interface");
}

TEST_F(CustomStructValueTest, Dispatcher_DebugString) {
  EXPECT_EQ(MakeDispatcher().DebugString(), "test.Dispatcher");
}

TEST_F(CustomStructValueTest, Interface_DebugString) {
  EXPECT_EQ(MakeInterface().DebugString(), "test.Interface");
}

TEST_F(CustomStructValueTest, Dispatcher_GetRuntimeType) {
  EXPECT_EQ(MakeDispatcher().GetRuntimeType(),
            common_internal::MakeBasicStructType("test.Dispatcher"));
}

TEST_F(CustomStructValueTest, Interface_GetRuntimeType) {
  EXPECT_EQ(MakeInterface().GetRuntimeType(),
            common_internal::MakeBasicStructType("test.Interface"));
}

TEST_F(CustomStructValueTest, Dispatcher_IsZeroValue) {
  EXPECT_FALSE(MakeDispatcher().IsZeroValue());
}

TEST_F(CustomStructValueTest, Interface_IsZeroValue) {
  EXPECT_FALSE(MakeInterface().IsZeroValue());
}

TEST_F(CustomStructValueTest, Dispatcher_SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(MakeDispatcher().SerializeTo(descriptor_pool(), message_factory(),
                                           &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), Not(IsEmpty()));
}

TEST_F(CustomStructValueTest, Interface_SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(MakeInterface().SerializeTo(descriptor_pool(), message_factory(),
                                          &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), Not(IsEmpty()));
}

TEST_F(CustomStructValueTest, Dispatcher_ConvertToJson) {
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

TEST_F(CustomStructValueTest, Interface_ConvertToJson) {
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

TEST_F(CustomStructValueTest, Dispatcher_ConvertToJsonObject) {
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

TEST_F(CustomStructValueTest, Interface_ConvertToJsonObject) {
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

TEST_F(CustomStructValueTest, Dispatcher_GetFieldByName) {
  EXPECT_THAT(MakeDispatcher().GetFieldByName("foo", descriptor_pool(),
                                              message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(MakeDispatcher().GetFieldByName("bar", descriptor_pool(),
                                              message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
}

TEST_F(CustomStructValueTest, Interface_GetFieldByName) {
  EXPECT_THAT(MakeInterface().GetFieldByName("foo", descriptor_pool(),
                                             message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(MakeInterface().GetFieldByName("bar", descriptor_pool(),
                                             message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
}

TEST_F(CustomStructValueTest, Dispatcher_GetFieldByNumber) {
  EXPECT_THAT(MakeDispatcher().GetFieldByNumber(1, descriptor_pool(),
                                                message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(MakeDispatcher().GetFieldByNumber(2, descriptor_pool(),
                                                message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
}

TEST_F(CustomStructValueTest, Interface_GetFieldByNumber) {
  EXPECT_THAT(MakeInterface().GetFieldByNumber(1, descriptor_pool(),
                                               message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(MakeInterface().GetFieldByNumber(2, descriptor_pool(),
                                               message_factory(), arena()),
              IsOkAndHolds(IntValueIs(1)));
}

TEST_F(CustomStructValueTest, Dispatcher_HasFieldByName) {
  EXPECT_THAT(MakeDispatcher().HasFieldByName("foo"), IsOkAndHolds(true));
  EXPECT_THAT(MakeDispatcher().HasFieldByName("bar"), IsOkAndHolds(true));
}

TEST_F(CustomStructValueTest, Interface_HasFieldByName) {
  EXPECT_THAT(MakeInterface().HasFieldByName("foo"), IsOkAndHolds(true));
  EXPECT_THAT(MakeInterface().HasFieldByName("bar"), IsOkAndHolds(true));
}

TEST_F(CustomStructValueTest, Dispatcher_HasFieldByNumber) {
  EXPECT_THAT(MakeDispatcher().HasFieldByNumber(1), IsOkAndHolds(true));
  EXPECT_THAT(MakeDispatcher().HasFieldByNumber(2), IsOkAndHolds(true));
}

TEST_F(CustomStructValueTest, Interface_HasFieldByNumber) {
  EXPECT_THAT(MakeInterface().HasFieldByNumber(1), IsOkAndHolds(true));
  EXPECT_THAT(MakeInterface().HasFieldByNumber(2), IsOkAndHolds(true));
}

TEST_F(CustomStructValueTest, Default_Bool) {
  EXPECT_FALSE(CustomStructValue());
}

TEST_F(CustomStructValueTest, Dispatcher_Bool) {
  EXPECT_TRUE(MakeDispatcher());
}

TEST_F(CustomStructValueTest, Interface_Bool) { EXPECT_TRUE(MakeInterface()); }

TEST_F(CustomStructValueTest, Dispatcher_ForEachField) {
  std::vector<std::pair<std::string, Value>> fields;
  EXPECT_THAT(MakeDispatcher().ForEachField(
                  [&](absl::string_view name,
                      const Value& value) -> absl::StatusOr<bool> {
                    fields.push_back(std::pair{std::string(name), value});
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(fields, UnorderedElementsAre(Pair("foo", BoolValueIs(true)),
                                           Pair("bar", IntValueIs(1))));
}

TEST_F(CustomStructValueTest, Interface_ForEachField) {
  std::vector<std::pair<std::string, Value>> fields;
  EXPECT_THAT(MakeInterface().ForEachField(
                  [&](absl::string_view name,
                      const Value& value) -> absl::StatusOr<bool> {
                    fields.push_back(std::pair{std::string(name), value});
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(fields, UnorderedElementsAre(Pair("foo", BoolValueIs(true)),
                                           Pair("bar", IntValueIs(1))));
}

TEST_F(CustomStructValueTest, Dispatcher_Qualify) {
  EXPECT_THAT(
      MakeDispatcher().Qualify({AttributeQualifier::OfString("foo")}, false,
                               descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_F(CustomStructValueTest, Interface_Qualify) {
  EXPECT_THAT(
      MakeInterface().Qualify({AttributeQualifier::OfString("foo")}, false,
                              descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_F(CustomStructValueTest, Dispatcher) {
  EXPECT_THAT(MakeDispatcher().dispatcher(), NotNull());
  EXPECT_THAT(MakeDispatcher().interface(), IsNull());
}

TEST_F(CustomStructValueTest, Interface) {
  EXPECT_THAT(MakeInterface().dispatcher(), IsNull());
  EXPECT_THAT(MakeInterface().interface(), NotNull());
}

}  // namespace
}  // namespace cel
