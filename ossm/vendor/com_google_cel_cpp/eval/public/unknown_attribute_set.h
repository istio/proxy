#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_

#include "base/attribute_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// UnknownAttributeSet is a container for CEL attributes that are identified as
// unknown during expression evaluation.
using UnknownAttributeSet = ::cel::AttributeSet;

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_UNKNOWN_ATTRIBUTE_SET_H_
