#include "eval/public/cel_attribute.h"

#include <string>

#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using google::api::expr::v1alpha1::Expr;

using ::absl_testing::StatusIs;
using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;

class DummyMap : public CelMap {
 public:
  absl::optional<CelValue> operator[](CelValue value) const override {
    return CelValue::CreateNull();
  }
  absl::StatusOr<const CelList*> ListKeys() const override {
    return absl::UnimplementedError("CelMap::ListKeys is not implemented");
  }

  int size() const override { return 0; }
};

class DummyList : public CelList {
 public:
  int size() const override { return 0; }

  CelValue operator[](int index) const override {
    return CelValue::CreateNull();
  }
};

TEST(CelAttributeQualifierTest, TestBoolAccess) {
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateBool(true));

  EXPECT_FALSE(qualifier.GetStringKey().has_value());
  EXPECT_FALSE(qualifier.GetInt64Key().has_value());
  EXPECT_FALSE(qualifier.GetUint64Key().has_value());
  EXPECT_TRUE(qualifier.GetBoolKey().has_value());
  EXPECT_THAT(qualifier.GetBoolKey().value(), Eq(true));
}

TEST(CelAttributeQualifierTest, TestInt64Access) {
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateInt64(1));

  EXPECT_FALSE(qualifier.GetBoolKey().has_value());
  EXPECT_FALSE(qualifier.GetStringKey().has_value());
  EXPECT_FALSE(qualifier.GetUint64Key().has_value());

  EXPECT_TRUE(qualifier.GetInt64Key().has_value());
  EXPECT_THAT(qualifier.GetInt64Key().value(), Eq(1));
}

TEST(CelAttributeQualifierTest, TestUint64Access) {
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateUint64(1));

  EXPECT_FALSE(qualifier.GetBoolKey().has_value());
  EXPECT_FALSE(qualifier.GetStringKey().has_value());
  EXPECT_FALSE(qualifier.GetInt64Key().has_value());

  EXPECT_TRUE(qualifier.GetUint64Key().has_value());
  EXPECT_THAT(qualifier.GetUint64Key().value(), Eq(1UL));
}

TEST(CelAttributeQualifierTest, TestStringAccess) {
  const std::string test = "test";
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateString(&test));

  EXPECT_FALSE(qualifier.GetBoolKey().has_value());
  EXPECT_FALSE(qualifier.GetInt64Key().has_value());
  EXPECT_FALSE(qualifier.GetUint64Key().has_value());

  EXPECT_TRUE(qualifier.GetStringKey().has_value());
  EXPECT_THAT(qualifier.GetStringKey().value(), Eq("test"));
}

void TestAllInequalities(const CelAttributeQualifier& qualifier) {
  EXPECT_FALSE(qualifier ==
               CreateCelAttributeQualifier(CelValue::CreateBool(false)));
  EXPECT_FALSE(qualifier ==
               CreateCelAttributeQualifier(CelValue::CreateInt64(0)));
  EXPECT_FALSE(qualifier ==
               CreateCelAttributeQualifier(CelValue::CreateUint64(0)));
  const std::string test = "Those are not the droids you are looking for.";
  EXPECT_FALSE(qualifier ==
               CreateCelAttributeQualifier(CelValue::CreateString(&test)));
}

TEST(CelAttributeQualifierTest, TestBoolComparison) {
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateBool(true));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CreateCelAttributeQualifier(CelValue::CreateBool(true)));
}

TEST(CelAttributeQualifierTest, TestInt64Comparison) {
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateInt64(true));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CreateCelAttributeQualifier(CelValue::CreateInt64(true)));
}

TEST(CelAttributeQualifierTest, TestUint64Comparison) {
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateUint64(true));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CreateCelAttributeQualifier(CelValue::CreateUint64(true)));
}

TEST(CelAttributeQualifierTest, TestStringComparison) {
  const std::string kTest = "test";
  auto qualifier = CreateCelAttributeQualifier(CelValue::CreateString(&kTest));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CreateCelAttributeQualifier(CelValue::CreateString(&kTest)));
}

void TestAllQualifierMismatches(const CelAttributeQualifierPattern& qualifier) {
  const std::string test = "Those are not the droids you are looking for.";
  EXPECT_FALSE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateBool(false))));
  EXPECT_FALSE(
      qualifier.IsMatch(CreateCelAttributeQualifier(CelValue::CreateInt64(0))));
  EXPECT_FALSE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateUint64(0))));
  EXPECT_FALSE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateString(&test))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierBoolMatch) {
  auto qualifier =
      CreateCelAttributeQualifierPattern(CelValue::CreateBool(true));

  TestAllQualifierMismatches(qualifier);

  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateBool(true))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierInt64Match) {
  auto qualifier = CreateCelAttributeQualifierPattern(CelValue::CreateInt64(1));

  TestAllQualifierMismatches(qualifier);
  EXPECT_TRUE(
      qualifier.IsMatch(CreateCelAttributeQualifier(CelValue::CreateInt64(1))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierUint64Match) {
  auto qualifier =
      CreateCelAttributeQualifierPattern(CelValue::CreateUint64(1));

  TestAllQualifierMismatches(qualifier);
  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateUint64(1))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierStringMatch) {
  const std::string test = "test";
  auto qualifier =
      CreateCelAttributeQualifierPattern(CelValue::CreateString(&test));

  TestAllQualifierMismatches(qualifier);

  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateString(&test))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierWildcardMatch) {
  auto qualifier = CelAttributeQualifierPattern::CreateWildcard();
  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateBool(false))));
  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateBool(true))));
  EXPECT_TRUE(
      qualifier.IsMatch(CreateCelAttributeQualifier(CelValue::CreateInt64(0))));
  EXPECT_TRUE(
      qualifier.IsMatch(CreateCelAttributeQualifier(CelValue::CreateInt64(1))));
  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateUint64(0))));
  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateUint64(1))));

  const std::string kTest1 = "test1";
  const std::string kTest2 = "test2";

  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateString(&kTest1))));
  EXPECT_TRUE(qualifier.IsMatch(
      CreateCelAttributeQualifier(CelValue::CreateString(&kTest2))));
}

TEST(CreateCelAttributePattern, Basic) {
  const std::string kTest = "def";
  CelAttributePattern pattern = CreateCelAttributePattern(
      "abc", {kTest, static_cast<uint64_t>(1), static_cast<int64_t>(-1), false,
              CelAttributeQualifierPattern::CreateWildcard()});

  EXPECT_THAT(pattern.variable(), Eq("abc"));
  ASSERT_THAT(pattern.qualifier_path(), SizeIs(5));
  EXPECT_TRUE(pattern.qualifier_path()[4].IsWildcard());
}

TEST(CreateCelAttributePattern, EmptyPath) {
  CelAttributePattern pattern = CreateCelAttributePattern("abc");

  EXPECT_THAT(pattern.variable(), Eq("abc"));
  EXPECT_THAT(pattern.qualifier_path(), IsEmpty());
}

TEST(CreateCelAttributePattern, Wildcards) {
  const std::string kTest = "*";
  CelAttributePattern pattern = CreateCelAttributePattern(
      "abc", {kTest, "false", CelAttributeQualifierPattern::CreateWildcard()});

  EXPECT_THAT(pattern.variable(), Eq("abc"));
  ASSERT_THAT(pattern.qualifier_path(), SizeIs(3));
  EXPECT_TRUE(pattern.qualifier_path()[0].IsWildcard());
  EXPECT_FALSE(pattern.qualifier_path()[1].IsWildcard());
  EXPECT_TRUE(pattern.qualifier_path()[2].IsWildcard());
}

TEST(CelAttribute, AsStringBasic) {
  CelAttribute attr(
      "var",
      {
          CreateCelAttributeQualifier(CelValue::CreateStringView("qual1")),
          CreateCelAttributeQualifier(CelValue::CreateStringView("qual2")),
          CreateCelAttributeQualifier(CelValue::CreateStringView("qual3")),
      });

  ASSERT_OK_AND_ASSIGN(std::string string_format, attr.AsString());

  EXPECT_EQ(string_format, "var.qual1.qual2.qual3");
}

TEST(CelAttribute, AsStringInvalidRoot) {
  CelAttribute attr(
      "", {
              CreateCelAttributeQualifier(CelValue::CreateStringView("qual1")),
              CreateCelAttributeQualifier(CelValue::CreateStringView("qual2")),
              CreateCelAttributeQualifier(CelValue::CreateStringView("qual3")),
          });

  EXPECT_EQ(attr.AsString().status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(CelAttribute, InvalidQualifiers) {
  Expr expr;
  expr.mutable_ident_expr()->set_name("var");
  google::protobuf::Arena arena;

  CelAttribute attr1("var", {
                                CreateCelAttributeQualifier(
                                    CelValue::CreateDuration(absl::Minutes(2))),
                            });
  CelAttribute attr2("var",
                     {
                         CreateCelAttributeQualifier(
                             CelProtoWrapper::CreateMessage(&expr, &arena)),
                     });
  CelAttribute attr3(
      "var", {
                 CreateCelAttributeQualifier(CelValue::CreateBool(false)),
             });

  // Implementation detail: Messages as attribute qualifiers are unsupported,
  // so the implementation treats them inequal to any other. This is included
  // for coverage.
  EXPECT_FALSE(attr1 == attr2);
  EXPECT_FALSE(attr2 == attr1);
  EXPECT_FALSE(attr2 == attr2);
  EXPECT_FALSE(attr1 == attr3);
  EXPECT_FALSE(attr3 == attr1);
  EXPECT_FALSE(attr2 == attr3);
  EXPECT_FALSE(attr3 == attr2);

  // If the attribute includes an unsupported qualifier, return invalid argument
  // error.
  EXPECT_THAT(attr1.AsString(), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(attr2.AsString(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(CelAttribute, AsStringQualiferTypes) {
  CelAttribute attr(
      "var",
      {
          CreateCelAttributeQualifier(CelValue::CreateStringView("qual1")),
          CreateCelAttributeQualifier(CelValue::CreateUint64(1)),
          CreateCelAttributeQualifier(CelValue::CreateInt64(-1)),
          CreateCelAttributeQualifier(CelValue::CreateBool(false)),
      });

  ASSERT_OK_AND_ASSIGN(std::string string_format, attr.AsString());

  EXPECT_EQ(string_format, "var.qual1[1][-1][false]");
}

}  // namespace
}  // namespace google::api::expr::runtime
