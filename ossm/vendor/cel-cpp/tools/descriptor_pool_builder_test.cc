// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/descriptor_pool_builder.h"

#include <utility>

#include "google/protobuf/descriptor.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "internal/testing.h"
#include "cel/expr/conformance/proto2/test_all_types.pb.h"
#include "cel/expr/conformance/proto2/test_all_types_extensions.pb.h"
#include "google/protobuf/text_format.h"

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::IsNull;
using ::testing::NotNull;

namespace cel {
namespace {

TEST(DescriptorPoolBuilderTest, IncludesDefaults) {
  DescriptorPoolBuilder builder;

  auto pool = std::move(builder).Build();
  EXPECT_THAT(
      pool->FindMessageTypeByName("cel.expr.conformance.proto2.TestAllTypes"),
      IsNull());

  EXPECT_THAT(pool->FindMessageTypeByName("google.protobuf.Timestamp"),
              NotNull());
  EXPECT_THAT(pool->FindMessageTypeByName("google.protobuf.Any"), NotNull());
}

TEST(DescriptorPoolBuilderTest, AddTransitiveDescriptorSet) {
  DescriptorPoolBuilder builder;
  ASSERT_THAT(builder.AddTransitiveDescriptorSet(
                  cel::expr::conformance::proto2::Proto2ExtensionScopedMessage::
                      descriptor()),
              IsOk());

  auto pool = std::move(builder).Build();
  EXPECT_THAT(
      pool->FindMessageTypeByName("cel.expr.conformance.proto2.TestAllTypes"),
      NotNull());
}

TEST(DescriptorPoolBuilderTest, AddTransitiveDescriptorSetSpan) {
  DescriptorPoolBuilder builder;
  const google::protobuf::Descriptor* descs[] = {
      cel::expr::conformance::proto2::TestAllTypes::descriptor(),
      cel::expr::conformance::proto2::Proto2ExtensionScopedMessage::
          descriptor()};
  ASSERT_THAT(builder.AddTransitiveDescriptorSet(descs), IsOk());

  auto pool = std::move(builder).Build();
  EXPECT_THAT(
      pool->FindMessageTypeByName("cel.expr.conformance.proto2.TestAllTypes"),
      NotNull());
}

TEST(DescriptorPoolBuilderTest, AddFileDescriptorSet) {
  DescriptorPoolBuilder builder;
  google::protobuf::FileDescriptorSet file_set;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "foo.proto"
        package: "cel.test"
        dependency: "bar.proto"
        message_type {
          name: "Foo"
          field: {
            name: "bar"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_MESSAGE
            type_name: ".cel.test.Bar"
          }
        }
      )pb",
      file_set.add_file()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "bar.proto"
        package: "cel.test"
        message_type {
          name: "Bar"
          field: {
            name: "baz"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_STRING
          }
        }
      )pb",
      file_set.add_file()));
  ASSERT_THAT(builder.AddFileDescriptorSet(file_set), IsOk());

  auto pool = std::move(builder).Build();
  EXPECT_THAT(pool->FindMessageTypeByName("cel.test.Foo"), NotNull());
  EXPECT_THAT(pool->FindMessageTypeByName("cel.test.Bar"), NotNull());
}

TEST(DescriptorPoolBuilderTest, BadRef) {
  DescriptorPoolBuilder builder;
  google::protobuf::FileDescriptorSet file_set;
  // Unfulfilled dependency.
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "foo.proto"
        package: "cel.test"
        dependency: "bar.proto"
        message_type {
          name: "Foo"
          field: {
            name: "bar"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_MESSAGE
            type_name: ".cel.test.Bar"
          }
        }
      )pb",
      file_set.add_file()));
  // Note: descriptor pool is initialized lazily so this will not lead to an
  // error now, but looking up the message will fail.
  ASSERT_THAT(builder.AddFileDescriptorSet(file_set), IsOk());

  auto pool = std::move(builder).Build();
  EXPECT_THAT(pool->FindMessageTypeByName("cel.test.Foo"), IsNull());
}

TEST(DescriptorPoolBuilderTest, AddFile) {
  DescriptorPoolBuilder builder;
  google::protobuf::FileDescriptorProto file;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "bar.proto"
        package: "cel.test"
        message_type {
          name: "Bar"
          field: {
            name: "baz"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_STRING
          }
        }
      )pb",
      &file));

  ASSERT_THAT(builder.AddFileDescriptor(file), IsOk());
  // Duplicate file.
  ASSERT_THAT(builder.AddFileDescriptor(file),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // In this specific case, we know that the duplicate is the same so
  // the pool will still be valid.
  auto pool = std::move(builder).Build();
  EXPECT_THAT(pool->FindMessageTypeByName("cel.test.Bar"), NotNull());
}

}  // namespace
}  // namespace cel
