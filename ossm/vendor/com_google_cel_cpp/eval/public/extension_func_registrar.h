#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_EXTENSION_FUNC_REGISTRAR_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_EXTENSION_FUNC_REGISTRAR_H_

#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Register generic/widely used extension functions.
absl::Status RegisterExtensionFunctions(CelFunctionRegistry* registry);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_EXTENSION_FUNC_REGISTRAR_H_
