#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "eval/public/cel_function_adapter_impl.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"

namespace google::api::expr::runtime {

namespace internal {

// A type code matcher that adds support for google::protobuf::Message.
struct ProtoAdapterTypeCodeMatcher {
  template <typename T>
  constexpr static std::optional<CelValue::Type> type_code() {
    if constexpr (std::is_same_v<T, const google::protobuf::Message*>) {
      return CelValue::Type::kMessage;
    } else {
      return internal::TypeCodeMatcher().type_code<T>();
    }
  }
};

// A value converter that handles wrapping google::protobuf::Messages as CelValues.
struct ProtoAdapterValueConverter
    : public internal::ValueConverterBase<ProtoAdapterValueConverter> {
  using BaseType = internal::ValueConverterBase<ProtoAdapterValueConverter>;
  using BaseType::NativeToValue;
  using BaseType::ValueToNative;

  absl::Status NativeToValue(const ::google::protobuf::Message* value,
                             ::google::protobuf::Arena* arena, CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null Message pointer returned");
    }
    *result = CelProtoWrapper::CreateMessage(value, arena);
    return absl::OkStatus();
  }
};
}  // namespace internal

// FunctionAdapter is a helper class that simplifies creation of CelFunction
// implementations.
//
// The static Create member function accepts CelFunction::Evalaute method
// implementations as std::function, allowing them to be lambdas/regular C++
// functions. CEL method descriptors ddeduced based on C++ function signatures.
//
// The adapted CelFunction::Evaluate implementation will set result to the
// value returned by the handler. To handle errors, choose CelValue as the
// return type, and use the CreateError/Create* helpers in cel_value.h.
//
// The wrapped std::function may return absl::StatusOr<v>. If the wrapped
// function returns the absl::Status variant, the generated CelFunction
// implementation will return a non-ok status code, rather than a CelError
// wrapping an absl::Status value. A returned non-ok status indicates a hard
// error, meaning the interpreter cannot reasonably continue evaluation (e.g.
// data corruption or broken invariant). To create a CelError that follows
// logical pruning rules, the extension function implementation should return a
// CelError or an error-typed CelValue.
//
// FunctionAdapter<ReturnType, ArgumentType...>
//   ReturnType: the C++ return type of the function implementation
//   Arguments: the C++ Argument type of the function implementation
//
// Static Methods:
//
// Create(absl::string_view function_name, bool receiver_style,
//          FunctionType func) -> absl::StatusOr<std::unique_ptr<CelFunction>>
//
// Usage example:
//
//  auto func = [](::google::protobuf::Arena* arena, int64_t i, int64_t j) -> bool {
//    return i < j;
//  };
//
//  CEL_ASSIGN_OR_RETURN(auto cel_func,
//      FunctionAdapter<bool, int64_t, int64_t>::Create("<", false, func));
//
// CreateAndRegister(absl::string_view function_name, bool receiver_style,
//                   FunctionType func, CelFunctionRegisry registry)
//      -> absl::Status
//
// Usage example:
//
//  auto func = [](::google::protobuf::Arena* arena, int64_t i, int64_t j) -> bool {
//    return i < j;
//  };
//
//  CEL_RETURN_IF_ERROR((
//      FunctionAdapter<bool, int64_t, int64_t>::CreateAndRegister("<", false,
//      func, cel_expression_builder->GetRegistry()));
//
template <typename ReturnType, typename... Arguments>
using FunctionAdapter =
    internal::FunctionAdapterImpl<internal::ProtoAdapterTypeCodeMatcher,
                                  internal::ProtoAdapterValueConverter>::
        FunctionAdapter<ReturnType, Arguments...>;

template <typename ReturnType, typename T>
using UnaryFunctionAdapter = internal::FunctionAdapterImpl<
    internal::ProtoAdapterTypeCodeMatcher,
    internal::ProtoAdapterValueConverter>::UnaryFunction<ReturnType, T>;

template <typename ReturnType, typename T, typename U>
using BinaryFunctionAdapter = internal::FunctionAdapterImpl<
    internal::ProtoAdapterTypeCodeMatcher,
    internal::ProtoAdapterValueConverter>::BinaryFunction<ReturnType, T, U>;

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_H_
