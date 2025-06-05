// Copyright 2021 Google LLC
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

#include "codelab/exercise2.h"

#include "google/rpc/context/attribute_context.pb.h"
#include "internal/testing.h"
#include "google/protobuf/text_format.h"

namespace google::api::expr::codelab {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::google::rpc::context::AttributeContext;
using ::google::protobuf::TextFormat;
using ::testing::HasSubstr;

TEST(Exercise2Var, Simple) {
  EXPECT_THAT(ParseAndEvaluate("bool_var", false), IsOkAndHolds(false));
  EXPECT_THAT(ParseAndEvaluate("bool_var", true), IsOkAndHolds(true));
  EXPECT_THAT(ParseAndEvaluate("bool_var || true", false), IsOkAndHolds(true));
  EXPECT_THAT(ParseAndEvaluate("bool_var && false", true), IsOkAndHolds(false));
}

TEST(Exercise2Var, WrongTypeResultError) {
  EXPECT_THAT(ParseAndEvaluate("'not a bool'", false),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected 'bool' result got 'string")));
}

TEST(Exercise2Context, Simple) {
  AttributeContext context;
  ASSERT_TRUE(TextFormat::ParseFromString(R"pb(
                                            source { ip: "192.168.28.1" }
                                            request { host: "www.example.com" }
                                            destination { ip: "192.168.56.1" }
                                          )pb",
                                          &context));

  EXPECT_THAT(ParseAndEvaluate("source.ip == '192.168.28.1'", context),
              IsOkAndHolds(true));
  EXPECT_THAT(ParseAndEvaluate("request.host == 'api.example.com'", context),
              IsOkAndHolds(false));
  EXPECT_THAT(ParseAndEvaluate("request.host == 'www.example.com'", context),
              IsOkAndHolds(true));
  EXPECT_THAT(ParseAndEvaluate("destination.ip != '192.168.56.1'", context),
              IsOkAndHolds(false));
}

TEST(Exercise2Context, WrongTypeResultError) {
  AttributeContext context;

  // For this codelab, we expect the bind default option which will return
  // proto api defaults for unset fields.
  EXPECT_THAT(ParseAndEvaluate("request.host", context),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected 'bool' result got 'string")));
}

}  // namespace
}  // namespace google::api::expr::codelab
