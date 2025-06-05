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

#include "common/memory.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/value.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace cel::extensions::test {
namespace {

using ::cel::extensions::ProtoMessageToValue;
using ::cel::internal::test::EqualsProto;
using ::google::api::expr::test::v1::proto2::TestAllTypes;

class ProtoValueTesting : public common_internal::ThreadCompatibleValueTest<> {
 protected:
  MemoryManager NewThreadCompatiblePoolingMemoryManager() override {
    return cel::extensions::ProtoMemoryManager(&arena_);
  }

 private:
  google::protobuf::Arena arena_;
};

class ProtoValueTestingTest : public ProtoValueTesting {};

TEST_P(ProtoValueTestingTest, StructValueAsProtoSimple) {
  TestAllTypes test_proto;
  test_proto.set_single_int32(42);
  test_proto.set_single_string("foo");

  ASSERT_OK_AND_ASSIGN(cel::Value v,
                       ProtoMessageToValue(value_manager(), test_proto));
  EXPECT_THAT(v, StructValueAsProto<TestAllTypes>(EqualsProto(R"pb(
                single_int32: 42
                single_string: "foo"
              )pb")));
}

INSTANTIATE_TEST_SUITE_P(ProtoValueTesting, ProtoValueTestingTest,
                         testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                         ProtoValueTestingTest::ToString);

}  // namespace
}  // namespace cel::extensions::test
