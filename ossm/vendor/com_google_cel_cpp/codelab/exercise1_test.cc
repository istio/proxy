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

#include "codelab/exercise1.h"

#include "internal/testing.h"

namespace google::api::expr::codelab {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;

TEST(Exercise1, PrintHelloWorld) {
  EXPECT_THAT(ParseAndEvaluate("'Hello, World!'"),
              IsOkAndHolds("Hello, World!"));
}

TEST(Exercise1, WrongTypeResultError) {
  EXPECT_THAT(ParseAndEvaluate("true"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "expected string result got 'bool'"));
}

TEST(Exercise1, Conditional) {
  EXPECT_THAT(ParseAndEvaluate("(1 < 0)? 'Hello, World!' : '¡Hola, Mundo!'"),
              IsOkAndHolds("¡Hola, Mundo!"));
}

}  // namespace
}  // namespace google::api::expr::codelab
