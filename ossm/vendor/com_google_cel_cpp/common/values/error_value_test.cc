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
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Not;

using ErrorValueTest = common_internal::ValueTest<>;

TEST_F(ErrorValueTest, Default) {
  ErrorValue value;
  EXPECT_THAT(value.NativeValue(), StatusIs(absl::StatusCode::kUnknown));
}

TEST_F(ErrorValueTest, OkStatus) {
  EXPECT_DEBUG_DEATH(static_cast<void>(ErrorValue(absl::OkStatus())), _);
}

TEST_F(ErrorValueTest, Kind) {
  EXPECT_EQ(ErrorValue(absl::CancelledError()).kind(), ErrorValue::kKind);
  EXPECT_EQ(Value(ErrorValue(absl::CancelledError())).kind(),
            ErrorValue::kKind);
}

TEST_F(ErrorValueTest, DebugString) {
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

TEST_F(ErrorValueTest, SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(
      ErrorValue().SerializeTo(descriptor_pool(), message_factory(), &output),
      StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ErrorValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(
      ErrorValue().ConvertToJson(descriptor_pool(), message_factory(), message),
      StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ErrorValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(ErrorValue(absl::CancelledError())),
            NativeTypeId::For<ErrorValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(ErrorValue(absl::CancelledError()))),
            NativeTypeId::For<ErrorValue>());
}

}  // namespace
}  // namespace cel
