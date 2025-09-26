#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_H_

#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/function_descriptor.h"
#include "common/value.h"
#include "eval/public/cel_value.h"
#include "runtime/function.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

// Type that describes CelFunction.
// This complex structure is needed for overloads support.
using CelFunctionDescriptor = ::cel::FunctionDescriptor;

// CelFunction is a handler that represents single
// CEL function.
// CelFunction provides Evaluate() method, that performs
// evaluation of the function. CelFunction instances provide
// descriptors that contain function information:
// - name
// - is function receiver style (e.f(g) vs f(e,g))
// - amount of arguments and their types.
// Function overloads are resolved based on their arguments and
// receiver style.
class CelFunction : public ::cel::Function {
 public:
  // Build CelFunction from descriptor
  explicit CelFunction(CelFunctionDescriptor descriptor)
      : descriptor_(std::move(descriptor)) {}

  // Non-copyable
  CelFunction(const CelFunction& other) = delete;
  CelFunction& operator=(const CelFunction& other) = delete;

  ~CelFunction() override = default;

  // Evaluates CelValue based on arguments supplied.
  // If result content is to be allocated (e.g. string concatenation),
  // arena parameter must be used as allocation manager.
  // Provides resulting value in *result, returns evaluation success/failure.
  // Methods should discriminate between internal evaluator errors, that
  // makes further evaluation impossible or unreasonable (example - argument
  // type or number mismatch) and business logic errors (example division by
  // zero). When former happens, error Status is returned and *result is
  // not changed. In case of business logic error, returned Status is Ok, and
  // error is provided as CelValue - wrapped CelError in *result.
  virtual absl::Status Evaluate(absl::Span<const CelValue> arguments,
                                CelValue* result,
                                google::protobuf::Arena* arena) const = 0;

  // Determines whether instance of CelFunction is applicable to
  // arguments supplied.
  // Method is called during runtime.
  bool MatchArguments(absl::Span<const CelValue> arguments) const;

  bool MatchArguments(absl::Span<const cel::Value> arguments) const;

  // Implements cel::Function.
  absl::StatusOr<cel::Value> Invoke(
      absl::Span<const cel::Value> arguments,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override;

  // CelFunction descriptor
  const CelFunctionDescriptor& descriptor() const { return descriptor_; }

 private:
  CelFunctionDescriptor descriptor_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_H_
