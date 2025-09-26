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
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;

using TypeValueTest = common_internal::ValueTest<>;

TEST_F(TypeValueTest, Kind) {
  EXPECT_EQ(TypeValue(AnyType()).kind(), TypeValue::kKind);
  EXPECT_EQ(Value(TypeValue(AnyType())).kind(), TypeValue::kKind);
}

TEST_F(TypeValueTest, DebugString) {
  {
    std::ostringstream out;
    out << TypeValue(AnyType());
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
  {
    std::ostringstream out;
    out << Value(TypeValue(AnyType()));
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
}

TEST_F(TypeValueTest, SerializeTo) {
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(TypeValue(AnyType()).SerializeTo(descriptor_pool(),
                                               message_factory(), &output),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(TypeValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(TypeValue(AnyType()).ConvertToJson(descriptor_pool(),
                                                 message_factory(), message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(TypeValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TypeValue(AnyType())),
            NativeTypeId::For<TypeValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(TypeValue(AnyType()))),
            NativeTypeId::For<TypeValue>());
}

}  // namespace
}  // namespace cel
