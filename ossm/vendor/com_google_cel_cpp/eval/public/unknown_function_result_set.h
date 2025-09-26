#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_

#include "base/function_result.h"
#include "base/function_result_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Represents a function result that is unknown at the time of execution. This
// allows for lazy evaluation of expensive functions.
using UnknownFunctionResult = ::cel::FunctionResult;

// Represents a collection of unknown function results at a particular point in
// execution. Execution should advance further if this set of unknowns are
// provided. It may not advance if only a subset are provided.
// Set semantics use |IsEqualTo()| defined on |UnknownFunctionResult|.
using UnknownFunctionResultSet = ::cel::FunctionResultSet;

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_FUNCTION_RESULT_SET_H_
