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

#include "extensions/protobuf/value_testing.h"

#include "common/value.h"
#include "common/value_testing.h"
#include "extensions/protobuf/value.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "cel/expr/conformance/proto2/test_all_types.pb.h"

namespace cel::extensions::test {
namespace {

using ::cel::expr::conformance::proto2::TestAllTypes;
using ::cel::extensions::ProtoMessageToValue;
using ::cel::internal::test::EqualsProto;

using ProtoValueTestingTest = common_internal::ValueTest<>;

TEST_F(ProtoValueTestingTest, StructValueAsProtoSimple) {
  TestAllTypes test_proto;
  test_proto.set_single_int32(42);
  test_proto.set_single_string("foo");

  ASSERT_OK_AND_ASSIGN(cel::Value v,
                       ProtoMessageToValue(test_proto, descriptor_pool(),
                                           message_factory(), arena()));
  EXPECT_THAT(v, StructValueAsProto<TestAllTypes>(EqualsProto(R"pb(
                single_int32: 42
                single_string: "foo"
              )pb")));
}

}  // namespace
}  // namespace cel::extensions::test
