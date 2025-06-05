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

#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;

using ErrorValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(ErrorValueTest, Default) {
  ErrorValue value;
  EXPECT_THAT(value.NativeValue(), StatusIs(absl::StatusCode::kUnknown));
}

TEST_P(ErrorValueTest, OkStatus) {
  EXPECT_DEBUG_DEATH(static_cast<void>(ErrorValue(absl::OkStatus())), _);
}

TEST_P(ErrorValueTest, Kind) {
  EXPECT_EQ(ErrorValue(absl::CancelledError()).kind(), ErrorValue::kKind);
  EXPECT_EQ(Value(ErrorValue(absl::CancelledError())).kind(),
            ErrorValue::kKind);
}

TEST_P(ErrorValueTest, DebugString) {
  {
    std::ostringstream out;
    out << ErrorValue(absl::CancelledError());
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
  {
    std::ostringstream out;
    out << Value(ErrorValue(absl::CancelledError()));
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
}

TEST_P(ErrorValueTest, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(ErrorValue().SerializeTo(value_manager(), value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ErrorValueTest, ConvertToJson) {
  EXPECT_THAT(ErrorValue().ConvertToJson(value_manager()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ErrorValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(ErrorValue(absl::CancelledError())),
            NativeTypeId::For<ErrorValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(ErrorValue(absl::CancelledError()))),
            NativeTypeId::For<ErrorValue>());
}

TEST_P(ErrorValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<ErrorValue>(ErrorValue(absl::CancelledError())));
  EXPECT_TRUE(
      InstanceOf<ErrorValue>(Value(ErrorValue(absl::CancelledError()))));
}

TEST_P(ErrorValueTest, Cast) {
  EXPECT_THAT(Cast<ErrorValue>(ErrorValue(absl::CancelledError())),
              An<ErrorValue>());
  EXPECT_THAT(Cast<ErrorValue>(Value(ErrorValue(absl::CancelledError()))),
              An<ErrorValue>());
}

TEST_P(ErrorValueTest, As) {
  EXPECT_THAT(As<ErrorValue>(Value(ErrorValue(absl::CancelledError()))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    ErrorValueTest, ErrorValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    ErrorValueTest::ToString);

}  // namespace
}  // namespace cel
