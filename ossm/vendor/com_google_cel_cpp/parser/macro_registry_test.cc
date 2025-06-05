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

#include "parser/macro_registry.h"

#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "internal/testing.h"
#include "parser/macro.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::Eq;
using ::testing::Ne;

TEST(MacroRegistry, RegisterAndFind) {
  MacroRegistry macros;
  EXPECT_THAT(macros.RegisterMacro(HasMacro()), IsOk());
  EXPECT_THAT(macros.FindMacro("has", 1, false), Ne(absl::nullopt));
}

TEST(MacroRegistry, RegisterRollsback) {
  MacroRegistry macros;
  EXPECT_THAT(macros.RegisterMacros({HasMacro(), AllMacro(), AllMacro()}),
              StatusIs(absl::StatusCode::kAlreadyExists));
  EXPECT_THAT(macros.FindMacro("has", 1, false), Eq(absl::nullopt));
}

}  // namespace
}  // namespace cel
