//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_IMPL_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace internal {
// TypeCodeMatch template helper.
// Used for CEL type deduction based on C++ native type.
struct TypeCodeMatcher {
  template <typename T>
  constexpr static std::optional<CelValue::Type> type_code() {
    if constexpr (std::is_same_v<T, CelValue>) {
      // A bit of a trick - to pass Any kind of value, we use generic CelValue
      // parameters.
      return CelValue::Type::kAny;
    } else {
      int index = CelValue::IndexOf<T>::value;
      if (index < 0) return {};
      CelValue::Type arg_type = static_cast<CelValue::Type>(index);
      if (arg_type >= CelValue::Type::kAny) {
        return {};
      }
      return arg_type;
    }
  }
};

// Template helper to construct an argument list for a CelFunctionDescriptor.
template <typename TypeCodeMatcher>
struct TypeAdder {
  template <int N, typename Type, typename... Args>
  bool AddType(std::vector<CelValue::Type>* arg_types) const {
    auto kind = TypeCodeMatcher().template type_code<Type>();
    if (!kind) {
      return false;
    }

    arg_types->push_back(*kind);

    return AddType<N, Args...>(arg_types);

    return true;
  }

  template <int N>
  bool AddType(std::vector<CelValue::Type>* arg_types) const {
    return true;
  }
};

// Template helper for C++ types to CEL conversions.
// Uses CRTP to dispatch to derived class overloads in the StatusOr helper.
template <class Derived>
struct ValueConverterBase {
  // Value to native uwraps a CelValue to a native type.
  template <typename T>
  bool ValueToNative(CelValue value, T* result) {
    if constexpr (std::is_same_v<T, CelValue>) {
      *result = std::move(value);
      return true;
    } else {
      return value.GetValue(result);
    }
  }

  // Native to value wraps a native return type to a CelValue.
  absl::Status NativeToValue(bool value, ::google::protobuf::Arena*, CelValue* result) {
    *result = CelValue::CreateBool(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(int64_t value, ::google::protobuf::Arena*,
                             CelValue* result) {
    *result = CelValue::CreateInt64(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(uint64_t value, ::google::protobuf::Arena*,
                             CelValue* result) {
    *result = CelValue::CreateUint64(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(double value, ::google::protobuf::Arena*, CelValue* result) {
    *result = CelValue::CreateDouble(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(CelValue::StringHolder value, ::google::protobuf::Arena*,
                             CelValue* result) {
    *result = CelValue::CreateString(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(CelValue::BytesHolder value, ::google::protobuf::Arena*,
                             CelValue* result) {
    *result = CelValue::CreateBytes(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(const CelList* value, ::google::protobuf::Arena*,
                             CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null CelList pointer returned");
    }
    *result = CelValue::CreateList(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(const CelMap* value, ::google::protobuf::Arena*,
                             CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null CelMap pointer returned");
    }
    *result = CelValue::CreateMap(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(CelValue::CelTypeHolder value, ::google::protobuf::Arena*,
                             CelValue* result) {
    *result = CelValue::CreateCelType(value);
    return absl::OkStatus();
  }

  absl::Status NativeToValue(const CelError* value, ::google::protobuf::Arena*,
                             CelValue* result) {
    if (value == nullptr) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Null CelError pointer returned");
    }
    *result = CelValue::CreateError(value);
    return absl::OkStatus();
  }

  // Special case -- just forward a CelValue.
  absl::Status NativeToValue(const CelValue& value, ::google::protobuf::Arena*,
                             CelValue* result) {
    *result = value;
    return absl::OkStatus();
  }

  template <typename T>
  absl::Status NativeToValue(absl::StatusOr<T> value, ::google::protobuf::Arena* arena,
                             CelValue* result) {
    CEL_ASSIGN_OR_RETURN(auto held_value, value);
    return Derived().NativeToValue(held_value, arena, result);
  }
};

struct ValueConverter : public ValueConverterBase<ValueConverter> {};

// Generalized implementation for function adapters. See comments on
// instantiated versions for details on usage.
//
// TypeCodeMatcher provides the mapping from C++ type to CEL type.
// ValueConverter provides value conversions from native to CEL and vice versa.
// ReturnType and Arguments types are instantiated for the particular shape of
// the adapted functions.
template <typename TypeCodeMatcher, typename ValueConverter>
class FunctionAdapterImpl {
 public:
  // Implementations for the common cases of unary and binary functions.
  // This reduces the binary size substantially over the generic templated
  // versions.
  template <typename ReturnType, typename T, typename U>
  class BinaryFunction : public CelFunction {
   public:
    using FuncType = std::function<ReturnType(::google::protobuf::Arena*, T, U)>;

    static std::unique_ptr<CelFunction> Create(absl::string_view name,
                                               bool receiver_style,
                                               FuncType handler) {
      constexpr auto arg1_type = TypeCodeMatcher::template type_code<T>();
      static_assert(arg1_type.has_value(), "T does not map to a CEL type.");
      constexpr auto arg2_type = TypeCodeMatcher::template type_code<U>();
      static_assert(arg2_type.has_value(), "U does not map to a CEL type.");
      std::vector<CelValue::Type> arg_types{*arg1_type, *arg2_type};

      return absl::WrapUnique(new BinaryFunction<ReturnType, T, U>(
          CelFunctionDescriptor(name, receiver_style, std::move(arg_types)),
          std::move(handler)));
    }

    absl::Status Evaluate(absl::Span<const CelValue> arguments,
                          CelValue* result,
                          google::protobuf::Arena* arena) const override {
      if (arguments.size() != 2) {
        return absl::InternalError("Argument number mismatch, expected 2");
      }
      T arg;
      if (!ValueConverter().ValueToNative(arguments[0], &arg)) {
        return absl::InternalError("C++ to CEL type conversion failed");
      }
      U arg2;
      if (!ValueConverter().ValueToNative(arguments[1], &arg2)) {
        return absl::InternalError("C++ to CEL type conversion failed");
      }
      ReturnType handlerResult = handler_(arena, arg, arg2);
      return ValueConverter().NativeToValue(handlerResult, arena, result);
    }

   private:
    BinaryFunction(CelFunctionDescriptor descriptor, FuncType handler)
        : CelFunction(descriptor), handler_(std::move(handler)) {}

    FuncType handler_;
  };

  template <typename ReturnType, typename T>
  class UnaryFunction : public CelFunction {
   public:
    using FuncType = std::function<ReturnType(::google::protobuf::Arena*, T)>;

    static std::unique_ptr<CelFunction> Create(absl::string_view name,
                                               bool receiver_style,
                                               FuncType handler) {
      constexpr auto arg_type = TypeCodeMatcher::template type_code<T>();
      static_assert(arg_type.has_value(), "T does not map to a CEL type.");
      std::vector<CelValue::Type> arg_types{*arg_type};

      return absl::WrapUnique(new UnaryFunction<ReturnType, T>(
          CelFunctionDescriptor(name, receiver_style, std::move(arg_types)),
          std::move(handler)));
    }

    absl::Status Evaluate(absl::Span<const CelValue> arguments,
                          CelValue* result,
                          google::protobuf::Arena* arena) const override {
      if (arguments.size() != 1) {
        return absl::InternalError("Argument number mismatch, expected 1");
      }
      T arg;
      if (!ValueConverter().ValueToNative(arguments[0], &arg)) {
        return absl::InternalError("C++ to CEL type conversion failed");
      }
      ReturnType handlerResult = handler_(arena, arg);
      return ValueConverter().NativeToValue(handlerResult, arena, result);
    }

   private:
    UnaryFunction(CelFunctionDescriptor descriptor, FuncType handler)
        : CelFunction(descriptor), handler_(std::move(handler)) {}

    FuncType handler_;
  };

  // Generalized implementation.
  template <typename ReturnType, typename... Arguments>
  class FunctionAdapter : public CelFunction {
   public:
    using FuncType = std::function<ReturnType(::google::protobuf::Arena*, Arguments...)>;
    using TypeAdder = internal::TypeAdder<TypeCodeMatcher>;

    FunctionAdapter(CelFunctionDescriptor descriptor, FuncType handler)
        : CelFunction(std::move(descriptor)), handler_(std::move(handler)) {}

    static absl::StatusOr<std::unique_ptr<CelFunction>> Create(
        absl::string_view name, bool receiver_type,
        std::function<ReturnType(::google::protobuf::Arena*, Arguments...)> handler) {
      std::vector<CelValue::Type> arg_types;
      arg_types.reserve(sizeof...(Arguments));

      if (!TypeAdder().template AddType<0, Arguments...>(&arg_types)) {
        return absl::Status(
            absl::StatusCode::kInternal,
            absl::StrCat("Failed to create adapter for ", name,
                         ": failed to determine input parameter type"));
      }

      return std::make_unique<FunctionAdapter>(
          CelFunctionDescriptor(name, receiver_type, std::move(arg_types)),
          std::move(handler));
    }

    // Creates function handler and attempts to register it with
    // supplied function registry.
    static absl::Status CreateAndRegister(
        absl::string_view name, bool receiver_type,
        std::function<ReturnType(::google::protobuf::Arena*, Arguments...)> handler,
        CelFunctionRegistry* registry) {
      CEL_ASSIGN_OR_RETURN(auto cel_function,
                           Create(name, receiver_type, std::move(handler)));

      return registry->Register(std::move(cel_function));
    }

#if defined(__clang__) || !defined(__GNUC__)
    template <int arg_index>
    inline absl::Status RunWrap(
        absl::Span<const CelValue> arguments,
        std::tuple<::google::protobuf::Arena*, Arguments...> input, CelValue* result,
        ::google::protobuf::Arena* arena) const {
      if (!ValueConverter().ValueToNative(arguments[arg_index],
                                          &std::get<arg_index + 1>(input))) {
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "Type conversion failed");
      }
      return RunWrap<arg_index + 1>(arguments, input, result, arena);
    }

    template <>
    inline absl::Status RunWrap<sizeof...(Arguments)>(
        absl::Span<const CelValue>,
        std::tuple<::google::protobuf::Arena*, Arguments...> input, CelValue* result,
        ::google::protobuf::Arena* arena) const {
      return ValueConverter().NativeToValue(absl::apply(handler_, input), arena,
                                            result);
    }
#else
    inline absl::Status RunWrap(
        std::function<ReturnType()> func,
        ABSL_ATTRIBUTE_UNUSED const absl::Span<const CelValue> argset,
        ::google::protobuf::Arena* arena, CelValue* result,
        ABSL_ATTRIBUTE_UNUSED int arg_index) const {
      return ValueConverter().NativeToValue(func(), arena, result);
    }

    template <typename Arg, typename... Args>
    inline absl::Status RunWrap(std::function<ReturnType(Arg, Args...)> func,
                                const absl::Span<const CelValue> argset,
                                ::google::protobuf::Arena* arena, CelValue* result,
                                int arg_index) const {
      Arg argument;
      if (!ValueConverter().ValueToNative(argset[arg_index], &argument)) {
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "Type conversion failed");
      }

      std::function<ReturnType(Args...)> wrapped_func =
          [func, argument](Args... args) -> ReturnType {
        return func(argument, args...);
      };

      return RunWrap(std::move(wrapped_func), argset, arena, result,
                     arg_index + 1);
    }
#endif

    absl::Status Evaluate(absl::Span<const CelValue> arguments,
                          CelValue* result,
                          ::google::protobuf::Arena* arena) const override {
      if (arguments.size() != sizeof...(Arguments)) {
        return absl::Status(absl::StatusCode::kInternal,
                            "Argument number mismatch");
      }

#if defined(__clang__) || !defined(__GNUC__)
      std::tuple<::google::protobuf::Arena*, Arguments...> input;
      std::get<0>(input) = arena;
      return RunWrap<0>(arguments, input, result, arena);
#else
      const auto* handler = &handler_;
      std::function<ReturnType(Arguments...)> wrapped_handler =
          [handler, arena](Arguments... args) -> ReturnType {
        return (*handler)(arena, args...);
      };
      return RunWrap(std::move(wrapped_handler), arguments, arena, result, 0);
#endif
    }

   private:
    FuncType handler_;
  };
};

}  // namespace internal

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_ADAPTER_IMPL_H_
