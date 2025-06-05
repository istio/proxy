#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_SET_UTIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_SET_UTIL_H_

#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Less than operator sufficient as a comparator in a set. This provides
// a stable and consistent but not necessarily meaningful ordering. This should
// not be used directly in the cel runtime (e.g. as an overload for _<_) as
// it conflicts with some of the expected behaviors.
//
// Type is compared using the the enum ordering for CelValue::Type then
// underlying values are compared:
//
// For lists, compares length first, then in-order elementwise compare.
//
// For maps, compares size first, then sorted key order elementwise compare
// (i.e. ((k1, v1) < (k2, v2))).
//
// For other types, it defaults to the wrapped value's operator<.
// Note that for For messages, errors, and unknown sets, this is a ptr
// comparison.
bool CelValueLessThan(CelValue lhs, CelValue rhs);
bool CelValueEqual(CelValue lhs, CelValue rhs);
bool CelValueGreaterThan(CelValue lhs, CelValue rhs);
int CelValueCompare(CelValue lhs, CelValue rhs);

// Convenience alias for using the CelValueLessThan function in sets providing
// the stl interface.
using CelValueLessThanComparator = decltype(&CelValueLessThan);
using CelValueEqualComparator = decltype(&CelValueEqual);
using CelValueGreaterThanComparator = decltype(&CelValueGreaterThan);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_SET_UTIL_H_
