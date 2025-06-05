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
#include <memory>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_manager.h"
#include "common/values/legacy_value_manager.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::MemoryManagerRef;
using ::cel::StructType;
using ::cel::Type;
using ::google::protobuf::Struct;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

MATCHER_P(TypeNameIs, name, "") {
  const Type& type = arg;
  *result_listener << "got typename: " << type.name();
  return type.name() == name;
}

MATCHER_P(MatchesEnumDescriptor, desc, "") {
  const auto& enum_type = arg;

  if (enum_type.enumerators.size() != desc->value_count()) {
    return false;
  }

  for (int i = 0; i < desc->value_count(); i++) {
    const auto& constant = enum_type.enumerators[i];

    const auto* value_desc = desc->value(i);

    if (value_desc->name() != constant.name) {
      return false;
    }
    if (value_desc->number() != constant.number) {
      return false;
    }
  }
  return true;
}

TEST(CelTypeRegistryTest, RegisterEnumDescriptor) {
  CelTypeRegistry registry;
  registry.Register(google::protobuf::GetEnumDescriptor<TestMessage::TestEnum>());

  EXPECT_THAT(
      registry.ListResolveableEnums(),
      UnorderedElementsAre("google.protobuf.NullValue",
                           "google.api.expr.runtime.TestMessage.TestEnum"));

  EXPECT_THAT(
      registry.resolveable_enums(),
      AllOf(Contains(Pair(
                "google.protobuf.NullValue",
                MatchesEnumDescriptor(
                    google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>()))),
            Contains(Pair(
                "google.api.expr.runtime.TestMessage.TestEnum",
                MatchesEnumDescriptor(
                    google::protobuf::GetEnumDescriptor<TestMessage::TestEnum>())))));
}

TEST(CelTypeRegistryTypeProviderTest, StructTypes) {
  CelTypeRegistry registry;
  google::protobuf::LinkMessageReflection<TestMessage>();
  google::protobuf::LinkMessageReflection<Struct>();

  registry.RegisterTypeProvider(std::make_unique<ProtobufDescriptorProvider>(
      google::protobuf::DescriptorPool::generated_pool(),
      google::protobuf::MessageFactory::generated_factory()));

  cel::common_internal::LegacyValueManager value_manager(
      MemoryManagerRef::ReferenceCounting(), registry.GetTypeProvider());

  ASSERT_OK_AND_ASSIGN(
      absl::optional<cel::Type> struct_message_type,
      value_manager.FindType("google.api.expr.runtime.TestMessage"));
  ASSERT_TRUE(struct_message_type.has_value());
  ASSERT_TRUE((*struct_message_type).Is<StructType>())
      << (*struct_message_type).DebugString();
  EXPECT_THAT(struct_message_type->As<StructType>()->name(),
              Eq("google.api.expr.runtime.TestMessage"));

  // Can't override builtins.
  ASSERT_OK_AND_ASSIGN(absl::optional<Type> struct_type,
                       value_manager.FindType("google.protobuf.Struct"));
  EXPECT_THAT(struct_type, Optional(TypeNameIs("map")));
}

}  // namespace
}  // namespace google::api::expr::runtime
