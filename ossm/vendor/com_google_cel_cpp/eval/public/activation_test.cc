#include "eval/public/activation.h"

#include <memory>
#include <string>
#include <utility>

#include "eval/eval/attribute_trail.h"
#include "eval/eval/ident_step.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::absl_testing::StatusIs;
using ::cel::extensions::ProtoMemoryManager;
using ::cel::expr::Expr;
using ::google::protobuf::Arena;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Return;

class MockValueProducer : public CelValueProducer {
 public:
  MOCK_METHOD(CelValue, Produce, (Arena*), (override));
};

// Simple function that takes no args and returns an int64.
class ConstCelFunction : public CelFunction {
 public:
  explicit ConstCelFunction(absl::string_view name)
      : CelFunction({std::string(name), false, {}}) {}
  explicit ConstCelFunction(const CelFunctionDescriptor& desc)
      : CelFunction(desc) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* output,
                        google::protobuf::Arena* arena) const override {
    *output = CelValue::CreateInt64(42);
    return absl::OkStatus();
  }
};

TEST(ActivationTest, CheckValueInsertFindAndRemove) {
  Activation activation;

  Arena arena;

  activation.InsertValue("value42", CelValue::CreateInt64(42));

  // Test getting unbound value
  EXPECT_FALSE(activation.FindValue("value43", &arena));

  // Test getting bound value
  EXPECT_TRUE(activation.FindValue("value42", &arena));

  CelValue value = activation.FindValue("value42", &arena).value();

  // Test value is correct.
  EXPECT_THAT(value.Int64OrDie(), Eq(42));

  // Test removing unbound value
  EXPECT_FALSE(activation.RemoveValueEntry("value43"));

  // Test removing bound value
  EXPECT_TRUE(activation.RemoveValueEntry("value42"));

  // Now the value is unbound
  EXPECT_FALSE(activation.FindValue("value42", &arena));
}

TEST(ActivationTest, CheckValueProducerInsertFindAndRemove) {
  const std::string kValue = "42";

  auto producer = std::make_unique<MockValueProducer>();

  google::protobuf::Arena arena;

  ON_CALL(*producer, Produce(&arena))
      .WillByDefault(Return(CelValue::CreateString(&kValue)));

  // ValueProducer is expected to be invoked only once.
  EXPECT_CALL(*producer, Produce(&arena)).Times(1);

  Activation activation;

  activation.InsertValueProducer("value42", std::move(producer));

  // Test getting unbound value
  EXPECT_FALSE(activation.FindValue("value43", &arena));

  // Test getting bound value - 1st pass

  // Access attempt is repeated twice.
  // ValueProducer is expected to be invoked only once.
  for (int i = 0; i < 2; i++) {
    auto opt_value = activation.FindValue("value42", &arena);
    EXPECT_TRUE(opt_value.has_value()) << " for pass " << i;
    CelValue value = opt_value.value();
    EXPECT_THAT(value.StringOrDie().value(), Eq(kValue)) << " for pass " << i;
  }

  // Test removing bound value
  EXPECT_TRUE(activation.RemoveValueEntry("value42"));

  // Now the value is unbound
  EXPECT_FALSE(activation.FindValue("value42", &arena));
}

TEST(ActivationTest, CheckInsertFunction) {
  Activation activation;
  ASSERT_OK(activation.InsertFunction(
      std::make_unique<ConstCelFunction>("ConstFunc")));

  auto overloads = activation.FindFunctionOverloads("ConstFunc");
  EXPECT_THAT(overloads,
              ElementsAre(Property(
                  &CelFunction::descriptor,
                  Property(&CelFunctionDescriptor::name, Eq("ConstFunc")))));

  EXPECT_THAT(activation.InsertFunction(
                  std::make_unique<ConstCelFunction>("ConstFunc")),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Function with same shape")));

  EXPECT_THAT(activation.FindFunctionOverloads("ConstFunc0"), IsEmpty());
}

TEST(ActivationTest, CheckRemoveFunction) {
  Activation activation;
  ASSERT_OK(activation.InsertFunction(std::make_unique<ConstCelFunction>(
      CelFunctionDescriptor{"ConstFunc", false, {CelValue::Type::kInt64}})));
  EXPECT_OK(activation.InsertFunction(std::make_unique<ConstCelFunction>(
      CelFunctionDescriptor{"ConstFunc", false, {CelValue::Type::kUint64}})));

  auto overloads = activation.FindFunctionOverloads("ConstFunc");
  EXPECT_THAT(
      overloads,
      ElementsAre(
          Property(&CelFunction::descriptor,
                   Property(&CelFunctionDescriptor::name, Eq("ConstFunc"))),
          Property(&CelFunction::descriptor,
                   Property(&CelFunctionDescriptor::name, Eq("ConstFunc")))));

  EXPECT_TRUE(activation.RemoveFunctionEntries(
      {"ConstFunc", false, {CelValue::Type::kAny}}));

  EXPECT_THAT(activation.FindFunctionOverloads("ConstFunc"), IsEmpty());
}

TEST(ActivationTest, CheckValueProducerClear) {
  const std::string kValue1 = "42";
  const std::string kValue2 = "43";

  auto producer1 = std::make_unique<MockValueProducer>();
  auto producer2 = std::make_unique<MockValueProducer>();

  google::protobuf::Arena arena;

  ON_CALL(*producer1, Produce(&arena))
      .WillByDefault(Return(CelValue::CreateString(&kValue1)));
  ON_CALL(*producer2, Produce(&arena))
      .WillByDefault(Return(CelValue::CreateString(&kValue2)));
  EXPECT_CALL(*producer1, Produce(&arena)).Times(2);
  EXPECT_CALL(*producer2, Produce(&arena)).Times(1);

  Activation activation;
  activation.InsertValueProducer("value42", std::move(producer1));
  activation.InsertValueProducer("value43", std::move(producer2));

  // Produce first value
  auto opt_value = activation.FindValue("value42", &arena);
  EXPECT_TRUE(opt_value.has_value());
  EXPECT_THAT(opt_value->StringOrDie().value(), Eq(kValue1));

  // Test clearing bound value
  EXPECT_TRUE(activation.ClearValueEntry("value42"));
  EXPECT_FALSE(activation.ClearValueEntry("value43"));

  // Produce second value
  auto opt_value2 = activation.FindValue("value43", &arena);
  EXPECT_TRUE(opt_value2.has_value());
  EXPECT_THAT(opt_value2->StringOrDie().value(), Eq(kValue2));

  // Clear all values
  EXPECT_EQ(1, activation.ClearCachedValues());
  EXPECT_FALSE(activation.ClearValueEntry("value42"));
  EXPECT_FALSE(activation.ClearValueEntry("value43"));

  // Produce first value again
  auto opt_value3 = activation.FindValue("value42", &arena);
  EXPECT_TRUE(opt_value3.has_value());
  EXPECT_THAT(opt_value3->StringOrDie().value(), Eq(kValue1));
  EXPECT_EQ(1, activation.ClearCachedValues());
}

TEST(ActivationTest, ErrorPathTest) {
  Activation activation;

  Expr expr;
  auto* select_expr = expr.mutable_select_expr();
  select_expr->set_field("ip");

  Expr* ident_expr = select_expr->mutable_operand();
  ident_expr->mutable_ident_expr()->set_name("destination");

  const CelAttributePattern destination_ip_pattern(
      "destination",
      {CreateCelAttributeQualifierPattern(CelValue::CreateStringView("ip"))});

  AttributeTrail trail("destination");
  trail =
      trail.Step(CreateCelAttributeQualifier(CelValue::CreateStringView("ip")));

  ASSERT_EQ(destination_ip_pattern.IsMatch(trail.attribute()),
            CelAttributePattern::MatchType::FULL);
  EXPECT_TRUE(activation.missing_attribute_patterns().empty());

  activation.set_missing_attribute_patterns({destination_ip_pattern});
  EXPECT_EQ(
      activation.missing_attribute_patterns()[0].IsMatch(trail.attribute()),
      CelAttributePattern::MatchType::FULL);
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
