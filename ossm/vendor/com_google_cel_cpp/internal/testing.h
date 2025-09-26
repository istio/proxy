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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_H_

#include "gmock/gmock.h"  // IWYU pragma: export
#include "gtest/gtest.h"  // IWYU pragma: export
#include "absl/status/status_matchers.h"
#include "internal/status_macros.h"  // IWYU pragma: keep

#ifndef ASSERT_OK
#define ASSERT_OK(expr) ASSERT_THAT(expr, ::absl_testing::IsOk())
#endif

#ifndef EXPECT_OK
#define EXPECT_OK(expr) EXPECT_THAT(expr, ::absl_testing::IsOk())
#endif

#ifndef ASSERT_OK_AND_ASSIGN
#define ASSERT_OK_AND_ASSIGN(lhs, rhs) \
  CEL_ASSIGN_OR_RETURN(                \
      lhs, rhs, ::cel::internal::AddFatalFailure(__FILE__, __LINE__, #rhs, _))
#endif

namespace cel::internal {

void AddFatalFailure(const char* file, int line, absl::string_view expression,
                     const StatusBuilder& builder);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_H_
