// Copyright 2023 Google LLC
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

#include "eval/eval/comprehension_slots.h"

#include "base/attribute.h"
#include "base/type_provider.h"
#include "common/memory.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

using ::cel::Attribute;

using ::absl_testing::IsOkAndHolds;
using ::cel::MemoryManagerRef;
using ::cel::StringValue;
using ::cel::TypeProvider;
using ::cel::Value;
using ::testing::Truly;

TEST(ComprehensionSlots, Basic) {
  ComprehensionSlots slots(4);

  ComprehensionSlots::Slot* slot0 = slots.Get(0);
  EXPECT_FALSE(slot0->Has());

  slots.Set(0, cel::StringValue("abcd"),
            AttributeTrail(Attribute("fake_attr")));

  ASSERT_TRUE(slot0->Has());

  EXPECT_THAT(slot0->value(), Truly([](const Value& v) {
                return v.Is<StringValue>() &&
                       v.GetString().ToString() == "abcd";
              }))
      << "value is 'abcd'";

  EXPECT_THAT(slot0->attribute().attribute().AsString(),
              IsOkAndHolds("fake_attr"));

  slots.ClearSlot(0);
  EXPECT_FALSE(slot0->Has());

  slots.Set(3, cel::StringValue("abcd"),
            AttributeTrail(Attribute("fake_attr")));

  auto* slot3 = slots.Get(3);

  ASSERT_TRUE(slot3->Has());
  EXPECT_THAT(slot3->value(), Truly([](const Value& v) {
                return v.Is<StringValue>() &&
                       v.GetString().ToString() == "abcd";
              }))
      << "value is 'abcd'";

  slots.Reset();
  EXPECT_FALSE(slot0->Has());
  EXPECT_FALSE(slot3->Has());
}

}  // namespace google::api::expr::runtime
