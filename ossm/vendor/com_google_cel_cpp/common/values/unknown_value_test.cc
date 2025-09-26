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

using UnknownValueTest = common_internal::ValueTest<>;

TEST_F(UnknownValueTest, Kind) {
  EXPECT_EQ(UnknownValue().kind(), UnknownValue::kKind);
  EXPECT_EQ(Value(UnknownValue()).kind(), UnknownValue::kKind);
}

TEST_F(UnknownValueTest, DebugString) {
  {
    std::ostringstream out;
    out << UnknownValue();
    EXPECT_EQ(out.str(), "");
  }
  {
    std::ostringstream out;
    out << Value(UnknownValue());
    EXPECT_EQ(out.str(), "");
  }
}

TEST_F(UnknownValueTest, SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(
      UnknownValue().SerializeTo(descriptor_pool(), message_factory(), &output),
      StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(UnknownValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(UnknownValue().ConvertToJson(descriptor_pool(), message_factory(),
                                           message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(UnknownValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UnknownValue()),
            NativeTypeId::For<UnknownValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(UnknownValue())),
            NativeTypeId::For<UnknownValue>());
}

}  // namespace
}  // namespace cel
