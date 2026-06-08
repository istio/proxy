// Copyright 2022 Google LLC
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

#include "codelab/exercise4.h"

#include "google/protobuf/struct.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "internal/testing.h"
#include "google/protobuf/text_format.h"

namespace cel_codelab {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::google::rpc::context::AttributeContext;

TEST(EvaluateWithExtensionFunction, Baseline) {
  AttributeContext context;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"(request {
        path: "/"
        auth {
          claims {
            fields {
              key: "group"
              value {string_value: "admin"}
            }
          }
        }
      })",
      &context));
  EXPECT_THAT(EvaluateWithExtensionFunction("request.path == '/'", context),
              IsOkAndHolds(true));
}

TEST(EvaluateWithExtensionFunction, ContainsTrue) {
  AttributeContext context;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"(request {
        path: "/"
        auth {
          claims {
            fields {
              key: "group"
              value {string_value: "admin"}
            }
          }
        }
      })",
      &context));
  EXPECT_THAT(EvaluateWithExtensionFunction(
                  "request.auth.claims.contains('group', 'admin')", context),
              IsOkAndHolds(true));
}

TEST(EvaluateWithExtensionFunction, ContainsFalse) {
  AttributeContext context;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"(request {
          path: "/"
      })",
      &context));
  EXPECT_THAT(EvaluateWithExtensionFunction(
                  "request.auth.claims.contains('group', 'admin')", context),
              IsOkAndHolds(false));
}

}  // namespace
}  // namespace cel_codelab
