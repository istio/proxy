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
#include "common/decl_proto.h"

#include <string>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/variant.h"
#include "common/decl.h"
#include "common/decl_proto_v1alpha1.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/text_format.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;

enum class DeclType { kVariable, kFunction, kInvalid };

struct TestCase {
  std::string proto_decl;
  DeclType decl_type;
};

class DeclFromProtoTest : public ::testing::TestWithParam<TestCase> {};

TEST_P(DeclFromProtoTest, FromProtoWorks) {
  const TestCase& test_case = GetParam();
  google::protobuf::Arena arena;
  const google::protobuf::DescriptorPool* descriptor_pool =
      google::protobuf::DescriptorPool::generated_pool();
  cel::expr::Decl decl_pb;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(test_case.proto_decl, &decl_pb));
  absl::StatusOr<absl::variant<VariableDecl, FunctionDecl>> decl_or =
      DeclFromProto(decl_pb, descriptor_pool, &arena);
  switch (test_case.decl_type) {
    case DeclType::kVariable: {
      ASSERT_OK_AND_ASSIGN(auto decl, decl_or);
      EXPECT_TRUE(absl::holds_alternative<VariableDecl>(decl));
      break;
    }
    case DeclType::kFunction: {
      ASSERT_OK_AND_ASSIGN(auto decl, decl_or);
      EXPECT_TRUE(absl::holds_alternative<FunctionDecl>(decl));
      break;
    }
    case DeclType::kInvalid: {
      EXPECT_THAT(decl_or, StatusIs(absl::StatusCode::kInvalidArgument));
      break;
    }
  }
}

// Tests that the v1alpha1 proto can be converted to the unversioned proto.
// Same underlying implementation.
TEST_P(DeclFromProtoTest, FromV1Alpha1ProtoWorks) {
  const TestCase& test_case = GetParam();
  google::protobuf::Arena arena;
  const google::protobuf::DescriptorPool* descriptor_pool =
      google::protobuf::DescriptorPool::generated_pool();
  google::api::expr::v1alpha1::Decl decl_pb;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(test_case.proto_decl, &decl_pb));
  absl::StatusOr<absl::variant<VariableDecl, FunctionDecl>> decl_or =
      DeclFromV1Alpha1Proto(decl_pb, descriptor_pool, &arena);
  switch (test_case.decl_type) {
    case DeclType::kVariable: {
      ASSERT_OK_AND_ASSIGN(auto decl, decl_or);
      EXPECT_TRUE(absl::holds_alternative<VariableDecl>(decl));
      break;
    }
    case DeclType::kFunction: {
      ASSERT_OK_AND_ASSIGN(auto decl, decl_or);
      EXPECT_TRUE(absl::holds_alternative<FunctionDecl>(decl));
      break;
    }
    case DeclType::kInvalid: {
      EXPECT_THAT(decl_or, StatusIs(absl::StatusCode::kInvalidArgument));
      break;
    }
  }
}

// TODO(uncreated-issue/80): Add tests for round-trip conversion after the ToProto
// functions are implemented.

INSTANTIATE_TEST_SUITE_P(
    DeclFromProtoTest, DeclFromProtoTest,
    testing::Values<TestCase>(
        TestCase{
            R"pb(
              name: "foo_var"
              ident { type { primitive: BOOL } })pb",
            DeclType::kVariable},
        TestCase{
            R"pb(
              name: "foo_fn"
              function {
                overloads {
                  overload_id: "foo_fn_int"
                  params { primitive: INT64 }
                  result_type { primitive: BOOL }
                }
                overloads {
                  overload_id: "int_foo_fn"
                  is_instance_function: true
                  params { primitive: INT64 }
                  result_type { primitive: BOOL }
                }
                overloads {
                  overload_id: "foo_fn_T"
                  params { type_param: "T" }
                  type_params: "T"
                  result_type { primitive: BOOL }
                }

              })pb",
            DeclType::kFunction},
        // Need a descriptor to lookup a struct type.
        TestCase{
            R"pb(
              name: "foo_fn"
              ident { type { message_type: "com.example.UnknownType" } })pb",
            DeclType::kInvalid},
        // Empty decl is invalid.
        TestCase{R"pb(name: "foo_fn")pb", DeclType::kInvalid}));

}  // namespace
}  // namespace cel
