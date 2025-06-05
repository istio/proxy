// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/legacy_type_adapter.h"

#include <vector>

#include "google/protobuf/arena.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

class TestAccessApiImpl : public LegacyTypeAccessApis {
 public:
  TestAccessApiImpl() {}
  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override {
    return absl::UnimplementedError("Not implemented");
  }

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManagerRef memory_manager) const override {
    return absl::UnimplementedError("Not implemented");
  }

  std::vector<absl::string_view> ListFields(
      const CelValue::MessageWrapper& instance) const override {
    return std::vector<absl::string_view>();
  }
};

TEST(LegacyTypeAdapterAccessApis, DefaultAlwaysInequal) {
  TestMessage message;
  MessageWrapper wrapper(&message, nullptr);
  MessageWrapper wrapper2(&message, nullptr);

  TestAccessApiImpl impl;

  EXPECT_FALSE(impl.IsEqualTo(wrapper, wrapper2));
}

}  // namespace
}  // namespace google::api::expr::runtime
