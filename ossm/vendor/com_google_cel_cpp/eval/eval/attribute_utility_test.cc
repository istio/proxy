#include "eval/eval/attribute_utility.h"

#include <string>
#include <vector>

#include "absl/types/span.h"
#include "base/attribute.h"
#include "base/attribute_set.h"
#include "common/unknown.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "internal/testing.h"
#include "runtime/internal/attribute_matcher.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

using ::cel::AttributeSet;

using ::cel::UnknownValue;
using ::cel::Value;
using ::testing::Eq;
using ::testing::SizeIs;
using ::testing::UnorderedPointwise;

class AttributeUtilityTest : public ::testing::Test {
 public:
  AttributeUtilityTest() = default;

 protected:
  google::protobuf::Arena arena_;
};

absl::Span<const CelAttributePattern> NoPatterns() { return {}; }

TEST_F(AttributeUtilityTest, UnknownsUtilityCheckUnknowns) {
  std::vector<CelAttributePattern> unknown_patterns = {
      CelAttributePattern("unknown0", {CreateCelAttributeQualifierPattern(
                                          CelValue::CreateInt64(1))}),
      CelAttributePattern("unknown0", {CreateCelAttributeQualifierPattern(
                                          CelValue::CreateInt64(2))}),
      CelAttributePattern("unknown1", {}),
      CelAttributePattern("unknown2", {}),
  };

  std::vector<CelAttributePattern> missing_attribute_patterns;

  AttributeUtility utility(unknown_patterns, missing_attribute_patterns);
  // no match for void trail
  ASSERT_FALSE(utility.CheckForUnknown(AttributeTrail(), true));
  ASSERT_FALSE(utility.CheckForUnknown(AttributeTrail(), false));

  AttributeTrail unknown_trail0("unknown0");

  { ASSERT_FALSE(utility.CheckForUnknown(unknown_trail0, false)); }

  { ASSERT_TRUE(utility.CheckForUnknown(unknown_trail0, true)); }

  {
    ASSERT_TRUE(utility.CheckForUnknown(
        unknown_trail0.Step(
            CreateCelAttributeQualifier(CelValue::CreateInt64(1))),
        false));
  }

  {
    ASSERT_TRUE(utility.CheckForUnknown(
        unknown_trail0.Step(
            CreateCelAttributeQualifier(CelValue::CreateInt64(1))),
        true));
  }
}

TEST_F(AttributeUtilityTest, UnknownsUtilityMergeUnknownsFromValues) {
  std::vector<CelAttributePattern> unknown_patterns;

  std::vector<CelAttributePattern> missing_attribute_patterns;

  CelAttribute attribute0("unknown0", {});
  CelAttribute attribute1("unknown1", {});

  AttributeUtility utility(unknown_patterns, missing_attribute_patterns);

  UnknownValue unknown_set0 =
      cel::UnknownValue(cel::Unknown(AttributeSet({attribute0})));
  UnknownValue unknown_set1 =
      cel::UnknownValue(cel::Unknown(AttributeSet({attribute1})));

  std::vector<cel::Value> values = {
      unknown_set0,
      unknown_set1,
      cel::BoolValue(true),
      cel::IntValue(1),
  };

  absl::optional<UnknownValue> unknown_set = utility.MergeUnknowns(values);
  ASSERT_TRUE(unknown_set.has_value());
  EXPECT_THAT((*unknown_set).attribute_set(),
              UnorderedPointwise(
                  Eq(), std::vector<CelAttribute>{attribute0, attribute1}));
}

TEST_F(AttributeUtilityTest, UnknownsUtilityCheckForUnknownsFromAttributes) {
  std::vector<CelAttributePattern> unknown_patterns = {
      CelAttributePattern("unknown0",
                          {CelAttributeQualifierPattern::CreateWildcard()}),
  };

  std::vector<CelAttributePattern> missing_attribute_patterns;

  AttributeTrail trail0("unknown0");
  AttributeTrail trail1("unknown1");

  CelAttribute attribute1("unknown1", {});
  UnknownSet unknown_set1(UnknownAttributeSet({attribute1}));

  AttributeUtility utility(unknown_patterns, missing_attribute_patterns);

  UnknownSet unknown_attr_set(utility.CheckForUnknowns(
      {
          AttributeTrail(),  // To make sure we handle empty trail gracefully.
          trail0.Step(CreateCelAttributeQualifier(CelValue::CreateInt64(1))),
          trail0.Step(CreateCelAttributeQualifier(CelValue::CreateInt64(2))),
      },
      false));

  UnknownSet unknown_set(unknown_set1, unknown_attr_set);

  ASSERT_THAT(unknown_set.unknown_attributes(), SizeIs(3));
}

TEST_F(AttributeUtilityTest, UnknownsUtilityCheckForMissingAttributes) {
  std::vector<CelAttributePattern> unknown_patterns;

  std::vector<CelAttributePattern> missing_attribute_patterns;

  AttributeTrail trail("destination");
  trail =
      trail.Step(CreateCelAttributeQualifier(CelValue::CreateStringView("ip")));

  AttributeUtility utility0(unknown_patterns, missing_attribute_patterns);
  EXPECT_FALSE(utility0.CheckForMissingAttribute(trail));

  missing_attribute_patterns.push_back(CelAttributePattern(
      "destination",
      {CreateCelAttributeQualifierPattern(CelValue::CreateStringView("ip"))}));

  AttributeUtility utility1(unknown_patterns, missing_attribute_patterns);
  EXPECT_TRUE(utility1.CheckForMissingAttribute(trail));
}

TEST_F(AttributeUtilityTest, CreateUnknownSet) {
  AttributeTrail trail("destination");
  trail =
      trail.Step(CreateCelAttributeQualifier(CelValue::CreateStringView("ip")));

  std::vector<CelAttributePattern> empty_patterns;
  AttributeUtility utility(empty_patterns, empty_patterns);

  UnknownValue set = utility.CreateUnknownSet(trail.attribute());
  ASSERT_THAT(set.attribute_set(), SizeIs(1));
  ASSERT_OK_AND_ASSIGN(auto elem, set.attribute_set().begin()->AsString());
  EXPECT_EQ(elem, "destination.ip");
}

class FakeMatcher : public cel::runtime_internal::AttributeMatcher {
 private:
  using MatchResult = cel::runtime_internal::AttributeMatcher::MatchResult;

 public:
  MatchResult CheckForUnknown(const cel::Attribute& attr) const override {
    std::string attr_str = attr.AsString().value_or("");
    if (attr_str == "device.foo") {
      return MatchResult::FULL;
    } else if (attr_str == "device") {
      return MatchResult::PARTIAL;
    }
    return MatchResult::NONE;
  }

  MatchResult CheckForMissing(const cel::Attribute& attr) const override {
    std::string attr_str = attr.AsString().value_or("");

    if (attr_str == "device2.foo") {
      return MatchResult::FULL;
    } else if (attr_str == "device2") {
      return MatchResult::PARTIAL;
    }
    return MatchResult::NONE;
  }
};

TEST_F(AttributeUtilityTest, CustomMatcher) {
  AttributeTrail trail("device");

  AttributeUtility utility(NoPatterns(), NoPatterns());
  FakeMatcher matcher;
  utility.set_matcher(&matcher);
  EXPECT_TRUE(utility.CheckForUnknownPartial(trail));
  EXPECT_FALSE(utility.CheckForUnknownExact(trail));

  trail = trail.Step(cel::AttributeQualifier::OfString("foo"));
  EXPECT_TRUE(utility.CheckForUnknownExact(trail));
  EXPECT_TRUE(utility.CheckForUnknownPartial(trail));

  trail = AttributeTrail("device2");
  EXPECT_FALSE(utility.CheckForMissingAttribute(trail));
  trail = trail.Step(cel::AttributeQualifier::OfString("foo"));
  EXPECT_TRUE(utility.CheckForMissingAttribute(trail));
}

}  // namespace google::api::expr::runtime
