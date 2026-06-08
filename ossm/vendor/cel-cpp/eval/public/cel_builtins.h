#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_BUILTINS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_BUILTINS_H_

#include "base/builtins.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Alias new namespace until external CEL users can be updated.
namespace builtin = cel::builtin;

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_BUILTINS_H_
