#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_

#include <memory>

#include "absl/base/attributes.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory creates CelExpressionBuilder implementation for public use.
std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const google::protobuf::DescriptorPool* descriptor_pool,
    google::protobuf::MessageFactory* message_factory,
    const InterpreterOptions& options = InterpreterOptions());

ABSL_DEPRECATED(
    "This overload uses the generated descriptor pool, which allows "
    "expressions to create any messages linked into the binary. This is not "
    "hermetic and potentially dangerous, you should select the descriptor pool "
    "carefully. Use the other overload and explicitly pass your descriptor "
    "pool. It can still be the generated descriptor pool, but the choice "
    "should be explicit. If you do not need struct creation, use "
    "`cel::GetMinimalDescriptorPool()`.")
inline std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const InterpreterOptions& options = InterpreterOptions()) {
  return CreateCelExpressionBuilder(google::protobuf::DescriptorPool::generated_pool(),
                                    google::protobuf::MessageFactory::generated_factory(),
                                    options);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_EXPR_BUILDER_FACTORY_H_
