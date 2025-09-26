#ifndef THIRD_PARTY_CEL_CPP_COMMON_OPERATORS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_OPERATORS_H_

#include <map>
#include <string>

#include "cel/expr/syntax.pb.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace google {
namespace api {
namespace expr {
namespace common {

// Operator function names.
struct CelOperator {
  static const char* CONDITIONAL;
  static const char* LOGICAL_AND;
  static const char* LOGICAL_OR;
  static const char* LOGICAL_NOT;
  static const char* IN_DEPRECATED;
  static const char* EQUALS;
  static const char* NOT_EQUALS;
  static const char* LESS;
  static const char* LESS_EQUALS;
  static const char* GREATER;
  static const char* GREATER_EQUALS;
  static const char* ADD;
  static const char* SUBTRACT;
  static const char* MULTIPLY;
  static const char* DIVIDE;
  static const char* MODULO;
  static const char* NEGATE;
  static const char* INDEX;
  // Macros
  static const char* HAS;
  static const char* ALL;
  static const char* EXISTS;
  static const char* EXISTS_ONE;
  static const char* MAP;
  static const char* FILTER;

  // Named operators, must not have be valid identifiers.
  static const char* NOT_STRICTLY_FALSE;
#pragma push_macro("IN")
#undef IN
  static const char* IN;
#pragma pop_macro("IN")

  static const absl::string_view OPT_INDEX;
  static const absl::string_view OPT_SELECT;
};

// These give access to all or some specific precedence value.
// Higher value means higher precedence, 0 means no precedence, i.e.,
// custom function and not builtin operator.
int LookupPrecedence(const std::string& op);

absl::optional<std::string> LookupUnaryOperator(const std::string& op);
absl::optional<std::string> LookupBinaryOperator(const std::string& op);
absl::optional<std::string> LookupOperator(const std::string& op);
absl::optional<std::string> ReverseLookupOperator(const std::string& op);

// returns true if op has a lower precedence than the one expressed in expr
bool IsOperatorLowerPrecedence(const std::string& op,
                               const cel::expr::Expr& expr);
// returns true if op has the same precedence as the one expressed in expr
bool IsOperatorSamePrecedence(const std::string& op,
                              const cel::expr::Expr& expr);
// return true if operator is left recursive, i.e., neither && nor ||.
bool IsOperatorLeftRecursive(const std::string& op);

}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_COMMON_OPERATORS_H_
