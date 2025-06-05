#include "common/operators.h"

#include <map>
#include <memory>
#include <string>

namespace google {
namespace api {
namespace expr {
namespace common {

namespace {
// These functions provide access to reverse mappings for operators.
// Functions generally map from text expression to Expr representation,
// e.g., from "&&" to "_&&_". Reverse operators provides a mapping from
// Expr to textual mapping, e.g., from "_&&_" to "&&".

const std::map<std::string, std::string>& UnaryOperators() {
  static std::shared_ptr<std::map<std::string, std::string>> unaries_map =
      [&]() {
        auto u = std::make_shared<std::map<std::string, std::string>>(
            std::map<std::string, std::string>{
                {CelOperator::NEGATE, "-"}, {CelOperator::LOGICAL_NOT, "!"}});
        return u;
      }();
  return *unaries_map;
}

const std::map<std::string, std::string>& BinaryOperators() {
  static std::shared_ptr<std::map<std::string, std::string>> binops_map =
      [&]() {
        auto c = std::make_shared<std::map<std::string, std::string>>(
            std::map<std::string, std::string>{
                {CelOperator::LOGICAL_OR, "||"},
                {CelOperator::LOGICAL_AND, "&&"},
                {CelOperator::LESS_EQUALS, "<="},
                {CelOperator::LESS, "<"},
                {CelOperator::GREATER_EQUALS, ">="},
                {CelOperator::GREATER, ">"},
                {CelOperator::EQUALS, "=="},
                {CelOperator::NOT_EQUALS, "!="},
                {CelOperator::IN_DEPRECATED, "in"},
                {CelOperator::IN, "in"},
                {CelOperator::ADD, "+"},
                {CelOperator::SUBTRACT, "-"},
                {CelOperator::MULTIPLY, "*"},
                {CelOperator::DIVIDE, "/"},
                {CelOperator::MODULO, "%"}});
        return c;
      }();
  return *binops_map;
}

const std::map<std::string, std::string>& ReverseOperators() {
  static std::shared_ptr<std::map<std::string, std::string>> operators_map =
      [&]() {
        auto c = std::make_shared<std::map<std::string, std::string>>(
            std::map<std::string, std::string>{
                {"+", CelOperator::ADD},
                {"-", CelOperator::SUBTRACT},
                {"*", CelOperator::MULTIPLY},
                {"/", CelOperator::DIVIDE},
                {"%", CelOperator::MODULO},
                {"==", CelOperator::EQUALS},
                {"!=", CelOperator::NOT_EQUALS},
                {">", CelOperator::GREATER},
                {">=", CelOperator::GREATER_EQUALS},
                {"<", CelOperator::LESS},
                {"<=", CelOperator::LESS_EQUALS},
                {"&&", CelOperator::LOGICAL_AND},
                {"!", CelOperator::LOGICAL_NOT},
                {"||", CelOperator::LOGICAL_OR},
                {"in", CelOperator::IN},
            });
        return c;
      }();
  return *operators_map;
}

const std::map<std::string, std::string>& Operators() {
  static std::shared_ptr<std::map<std::string, std::string>> operators_map =
      [&]() {
        auto c = std::make_shared<std::map<std::string, std::string>>(
            std::map<std::string, std::string>{
                {CelOperator::ADD, "+"},
                {CelOperator::SUBTRACT, "-"},
                {CelOperator::MULTIPLY, "*"},
                {CelOperator::DIVIDE, "/"},
                {CelOperator::MODULO, "%"},
                {CelOperator::EQUALS, "=="},
                {CelOperator::NOT_EQUALS, "!="},
                {CelOperator::GREATER, ">"},
                {CelOperator::GREATER_EQUALS, ">="},
                {CelOperator::LESS, "<"},
                {CelOperator::LESS_EQUALS, "<="},
                {CelOperator::LOGICAL_AND, "&&"},
                {CelOperator::LOGICAL_NOT, "!"},
                {CelOperator::LOGICAL_OR, "||"},
                {CelOperator::IN, "in"},
                {CelOperator::IN_DEPRECATED, "in"},
                {CelOperator::NEGATE, "-"}});
        return c;
      }();
  return *operators_map;
}

// precedence of the operator, where the higher value means higher.
const std::map<std::string, int>& Precedences() {
  static std::shared_ptr<std::map<std::string, int>> precedence_map = [&]() {
    auto c = std::make_shared<std::map<std::string, int>>(
        std::map<std::string, int>{{CelOperator::CONDITIONAL, 8},

                                   {CelOperator::LOGICAL_OR, 7},

                                   {CelOperator::LOGICAL_AND, 6},

                                   {CelOperator::EQUALS, 5},
                                   {CelOperator::GREATER, 5},
                                   {CelOperator::GREATER_EQUALS, 5},
                                   {CelOperator::IN, 5},
                                   {CelOperator::LESS, 5},
                                   {CelOperator::LESS_EQUALS, 5},
                                   {CelOperator::NOT_EQUALS, 5},
                                   {CelOperator::IN_DEPRECATED, 5},

                                   {CelOperator::ADD, 4},
                                   {CelOperator::SUBTRACT, 4},

                                   {CelOperator::DIVIDE, 3},
                                   {CelOperator::MODULO, 3},
                                   {CelOperator::MULTIPLY, 3},

                                   {CelOperator::LOGICAL_NOT, 2},
                                   {CelOperator::NEGATE, 2},

                                   {CelOperator::INDEX, 1}});
    return c;
  }();
  return *precedence_map;
}

}  // namespace

const char* CelOperator::CONDITIONAL = "_?_:_";
const char* CelOperator::LOGICAL_AND = "_&&_";
const char* CelOperator::LOGICAL_OR = "_||_";
const char* CelOperator::LOGICAL_NOT = "!_";
const char* CelOperator::IN_DEPRECATED = "_in_";
const char* CelOperator::EQUALS = "_==_";
const char* CelOperator::NOT_EQUALS = "_!=_";
const char* CelOperator::LESS = "_<_";
const char* CelOperator::LESS_EQUALS = "_<=_";
const char* CelOperator::GREATER = "_>_";
const char* CelOperator::GREATER_EQUALS = "_>=_";
const char* CelOperator::ADD = "_+_";
const char* CelOperator::SUBTRACT = "_-_";
const char* CelOperator::MULTIPLY = "_*_";
const char* CelOperator::DIVIDE = "_/_";
const char* CelOperator::MODULO = "_%_";
const char* CelOperator::NEGATE = "-_";
const char* CelOperator::INDEX = "_[_]";
const char* CelOperator::HAS = "has";
const char* CelOperator::ALL = "all";
const char* CelOperator::EXISTS = "exists";
const char* CelOperator::EXISTS_ONE = "exists_one";
const char* CelOperator::MAP = "map";
const char* CelOperator::FILTER = "filter";
const char* CelOperator::NOT_STRICTLY_FALSE = "@not_strictly_false";
const char* CelOperator::IN = "@in";

const absl::string_view CelOperator::OPT_INDEX = "_[?_]";
const absl::string_view CelOperator::OPT_SELECT = "_?._";

int LookupPrecedence(const std::string& op) {
  auto precs = Precedences();
  auto p = precs.find(op);
  if (p != precs.end()) {
    return p->second;
  }
  return 0;
}

absl::optional<std::string> LookupUnaryOperator(const std::string& op) {
  auto unary_ops = UnaryOperators();
  auto o = unary_ops.find(op);
  if (o == unary_ops.end()) {
    return absl::optional<std::string>();
  }
  return o->second;
}

absl::optional<std::string> LookupBinaryOperator(const std::string& op) {
  auto bin_ops = BinaryOperators();
  auto o = bin_ops.find(op);
  if (o == bin_ops.end()) {
    return absl::optional<std::string>();
  }
  return o->second;
}

absl::optional<std::string> LookupOperator(const std::string& op) {
  auto ops = Operators();
  auto o = ops.find(op);
  if (o == ops.end()) {
    return absl::optional<std::string>();
  }
  return o->second;
}

absl::optional<std::string> ReverseLookupOperator(const std::string& op) {
  auto rev_ops = ReverseOperators();
  auto o = rev_ops.find(op);
  if (o == rev_ops.end()) {
    return absl::optional<std::string>();
  }
  return o->second;
}

bool IsOperatorSamePrecedence(const std::string& op,
                              const google::api::expr::v1alpha1::Expr& expr) {
  if (!expr.has_call_expr()) {
    return false;
  }
  return LookupPrecedence(op) == LookupPrecedence(expr.call_expr().function());
}

bool IsOperatorLowerPrecedence(const std::string& op,
                               const google::api::expr::v1alpha1::Expr& expr) {
  if (!expr.has_call_expr()) {
    return false;
  }
  return LookupPrecedence(op) < LookupPrecedence(expr.call_expr().function());
}

bool IsOperatorLeftRecursive(const std::string& op) {
  return op != CelOperator::LOGICAL_AND && op != CelOperator::LOGICAL_OR;
}

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
