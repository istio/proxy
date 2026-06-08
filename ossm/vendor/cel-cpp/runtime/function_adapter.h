// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Definitions for template helpers to wrap C++ functions as CEL extension
// function implementations.

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_ADAPTER_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/function_descriptor.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/function.h"
#include "runtime/internal/function_adapter.h"
#include "runtime/register_function_helper.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

namespace runtime_internal {

template <typename T>
struct AdaptedTypeTraits {
  using AssignableType = T;

  static T ToArg(AssignableType v) { return v; }
};

// Specialization for cref parameters without forcing a temporary copy of the
// underlying handle argument.
template <>
struct AdaptedTypeTraits<const Value&> {
  using AssignableType = const Value*;

  static std::reference_wrapper<const Value> ToArg(AssignableType v) {
    return *v;
  }
};

template <>
struct AdaptedTypeTraits<const StringValue&> {
  using AssignableType = const StringValue*;

  static std::reference_wrapper<const StringValue> ToArg(AssignableType v) {
    return *v;
  }
};

template <>
struct AdaptedTypeTraits<const BytesValue&> {
  using AssignableType = const BytesValue*;

  static std::reference_wrapper<const BytesValue> ToArg(AssignableType v) {
    return *v;
  }
};

// Partial specialization for other cases.
//
// These types aren't referenceable since they aren't actually
// represented as alternatives in the underlying variant.
//
// This still requires an implicit copy and corresponding ref-count increase.
template <typename T>
struct AdaptedTypeTraits<const T&> {
  using AssignableType = T;

  static T ToArg(AssignableType v) { return v; }
};

template <size_t I, typename... Args>
struct AdaptHelperImpl {
  template <typename T>
  static absl::Status Apply(absl::Span<const Value> input, T& output) {
    static_assert(sizeof...(Args) > 0);
    static_assert(std::tuple_size_v<T> == sizeof...(Args));
    CEL_RETURN_IF_ERROR(ValueToAdaptedVisitor{input[I]}(&std::get<I>(output)));
    if constexpr (I == sizeof...(Args) - 1) {
      return absl::OkStatus();
    } else {
      CEL_RETURN_IF_ERROR(
          (AdaptHelperImpl<I + 1, Args...>::template Apply<T>(input, output)));
    }
    return absl::OkStatus();
  }
};

template <typename... Args>
struct AdaptHelper {
  template <typename T>
  static absl::Status Apply(absl::Span<const Value> input, T& output) {
    return AdaptHelperImpl<0, Args...>::template Apply<T>(input, output);
  }
};

template <typename... Args>
struct ToArgsImpl {
  template <int I, typename T>
  struct El {
    using type = T;
    constexpr static size_t index = I;
  };

  template <typename... Es>
  struct ZipHolder {
    template <typename ResultType, typename TupleType, typename Op>
    static ResultType ToArgs(
        Op&& op, const TupleType& argbuffer,
        const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
        google::protobuf::MessageFactory* absl_nonnull message_factory,
        google::protobuf::Arena* absl_nonnull arena) {
      return std::forward<Op>(op)(
          runtime_internal::AdaptedTypeTraits<typename Es::type>::ToArg(
              std::get<Es::index>(argbuffer))...,
          descriptor_pool, message_factory, arena);
    }
  };

  template <size_t... Is>
  static ZipHolder<El<Is, Args>...> MakeZip(const std::index_sequence<Is...>&) {
    return ZipHolder<El<Is, Args>...>{};
  }
};

template <typename... Args>
struct ToArgsHelper {
  template <typename ResultType, typename TupleType, typename Op>
  static ResultType Apply(
      Op&& op, const TupleType& argbuffer,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) {
    using Impl = ToArgsImpl<Args...>;
    using Zip = decltype(Impl::MakeZip(std::index_sequence_for<Args...>{}));
    return Zip::template ToArgs<ResultType>(std::forward<Op>(op), argbuffer,
                                            descriptor_pool, message_factory,
                                            arena);
  }
};

}  // namespace runtime_internal

// Adapter class for generating CEL extension functions from a one argument
// function.
//
// See documentation for Binary Function adapter for general recommendations.
//
// Example Usage:
//  double Invert(ValueManager&, double x) {
//   return 1 / x;
//  }
//
//  {
//    std::unique_ptr<CelExpressionBuilder> builder;
//
//    CEL_RETURN_IF_ERROR(
//      builder->GetRegistry()->Register(
//        UnaryFunctionAdapter<double, double>::CreateDescriptor("inv",
//        /*receiver_style=*/false),
//         UnaryFunctionAdapter<double, double>::WrapFunction(&Invert)));
//  }
//  // example CEL expression
//  inv(4) == 1/4 [true]
template <typename T>
class NullaryFunctionAdapter
    : public RegisterHelper<NullaryFunctionAdapter<T>> {
 public:
  using FunctionType = absl::AnyInvocable<T(
      const google::protobuf::DescriptorPool* absl_nonnull,
      google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull) const>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<UnaryFunctionImpl>(std::move(fn));
  }

  template <typename F, typename = std::enable_if_t<std::is_invocable_v<F>>>
  static std::unique_ptr<cel::Function> WrapFunction(F&& function) {
    return WrapFunction(
        [function = std::forward<F>(function)](
            const google::protobuf::DescriptorPool* absl_nonnull,
            google::protobuf::MessageFactory* absl_nonnull,
            google::protobuf::Arena* absl_nonnull) -> T { return function(); });
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict) {
    return CreateDescriptor(name, receiver_style,
                            {is_strict, /*is_contextual=*/false});
  }

  static FunctionDescriptor CreateDescriptor(
      absl::string_view name, bool receiver_style,
      FunctionDescriptorOptions options = {}) {
    return FunctionDescriptor(name, receiver_style, {}, options);
  }

 private:
  class UnaryFunctionImpl : public cel::Function {
   public:
    explicit UnaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Value> Invoke(
        absl::Span<const Value> args,
        const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
        google::protobuf::MessageFactory* absl_nonnull message_factory,
        google::protobuf::Arena* absl_nonnull arena) const override {
      if (args.size() != 0) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for nullary function");
      }

      if constexpr (std::is_same_v<T, Value> ||
                    std::is_same_v<T, absl::StatusOr<Value>>) {
        return fn_(descriptor_pool, message_factory, arena);
      } else {
        T result = fn_(descriptor_pool, message_factory, arena);

        return runtime_internal::AdaptedToValueVisitor{}(std::move(result));
      }
    }

   private:
    FunctionType fn_;
  };
};

// Adapter class for generating CEL extension functions from a one argument
// function.
//
// See documentation for Binary Function adapter for general recommendations.
//
// Example Usage:
//  double Invert(ValueManager&, double x) {
//   return 1 / x;
//  }
//
//  {
//    std::unique_ptr<CelExpressionBuilder> builder;
//
//    CEL_RETURN_IF_ERROR(
//      builder->GetRegistry()->Register(
//        UnaryFunctionAdapter<double, double>::CreateDescriptor("inv",
//        /*receiver_style=*/false),
//         UnaryFunctionAdapter<double, double>::WrapFunction(&Invert)));
//  }
//  // example CEL expression
//  inv(4) == 1/4 [true]
template <typename T, typename U>
class UnaryFunctionAdapter : public RegisterHelper<UnaryFunctionAdapter<T, U>> {
 public:
  using FunctionType = absl::AnyInvocable<T(
      U, const google::protobuf::DescriptorPool* absl_nonnull,
      google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull) const>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<UnaryFunctionImpl>(std::move(fn));
  }

  template <typename F, typename = std::enable_if_t<std::is_invocable_v<F, U>>>
  static std::unique_ptr<cel::Function> WrapFunction(F&& function) {
    return WrapFunction(
        [function = std::forward<F>(function)](
            U arg1, const google::protobuf::DescriptorPool* absl_nonnull,
            google::protobuf::MessageFactory* absl_nonnull,
            google::protobuf::Arena* absl_nonnull) -> T { return function(arg1); });
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict) {
    return CreateDescriptor(
        name, receiver_style,
        FunctionDescriptorOptions{is_strict, /*is_contextual=*/false});
  }

  static FunctionDescriptor CreateDescriptor(
      absl::string_view name, bool receiver_style,
      FunctionDescriptorOptions options = {}) {
    return FunctionDescriptor(name, receiver_style,
                              {runtime_internal::AdaptedKind<U>()}, options);
  }

 private:
  class UnaryFunctionImpl : public cel::Function {
   public:
    explicit UnaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Value> Invoke(
        absl::Span<const Value> args,
        const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
        google::protobuf::MessageFactory* absl_nonnull message_factory,
        google::protobuf::Arena* absl_nonnull arena) const override {
      using ArgTraits = runtime_internal::AdaptedTypeTraits<U>;
      if (args.size() != 1) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for unary function");
      }
      typename ArgTraits::AssignableType arg1;

      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[0]}(&arg1));
      if constexpr (std::is_same_v<T, Value> ||
                    std::is_same_v<T, absl::StatusOr<Value>>) {
        return fn_(ArgTraits::ToArg(arg1), descriptor_pool, message_factory,
                   arena);
      } else {
        T result = fn_(ArgTraits::ToArg(arg1), descriptor_pool, message_factory,
                       arena);

        return runtime_internal::AdaptedToValueVisitor{}(std::move(result));
      }
    }

   private:
    FunctionType fn_;
  };
};

// Adapter class for generating CEL extension functions from a two argument
// function. Generates an implementation of the cel::Function interface that
// calls the function to wrap.
//
// Extension functions must distinguish between recoverable errors (error that
// should participate in CEL's error pruning) and unrecoverable errors (a non-ok
// absl::Status that stops evaluation). The function to wrap may return
// StatusOr<T> to propagate a Status, or return a Value with an Error
// value to introduce a CEL error.
//
// To introduce an extension function that may accept any kind of CEL value as
// an argument, the wrapped function should use a Value<Handle> parameter and
// check the type of the argument at evaluation time.
//
// Supported CEL to C++ type mappings:
// bool -> bool
// double -> double
// uint -> uint64_t
// int -> int64_t
// timestamp -> absl::Time
// duration -> absl::Duration
//
// Complex types may be referred to by cref or value.
// To return these, users should return a Value.
// any/dyn -> Value, const Value&
// string -> StringValue | const StringValue&
// bytes -> BytesValue | const BytesValue&
// list -> ListValue | const ListValue&
// map -> MapValue | const MapValue&
// struct -> StructValue | const StructValue&
// null -> NullValue | const NullValue&
//
// To intercept error and unknown arguments, users must use a non-strict
// overload with all arguments typed as any and check the kind of the
// Value argument.
//
// Example Usage:
//  double SquareDifference(ValueManager&, double x, double y) {
//    return x * x - y * y;
//  }
//
//  {
//    RuntimeBuilder builder;
//    // Initialize Expression builder with built-ins as needed.
//
//    CEL_RETURN_IF_ERROR(
//      builder.function_registry().Register(
//        BinaryFunctionAdapter<double, double, double>::CreateDescriptor(
//          "sq_diff", /*receiver_style=*/false),
//        BinaryFunctionAdapter<double, double, double>::WrapFunction(
//          &SquareDifference)));
//
//
//    // Alternative shorthand
//    // See RegisterHelper (template base class) for details.
//    // runtime/register_function_helper.h
//    auto status = BinaryFunctionAdapter<double, double, double>::
//        RegisterGlobalOverload(
//            "sq_diff",
//            &SquareDifference,
//            builder.function_registry());
//    CEL_RETURN_IF_ERROR(status);
//  }
//
// example CEL expression:
//  sq_diff(4, 3) == 7 [true]
//
template <typename T, typename U, typename V>
class BinaryFunctionAdapter
    : public RegisterHelper<BinaryFunctionAdapter<T, U, V>> {
 public:
  using FunctionType = absl::AnyInvocable<T(
      U, V, const google::protobuf::DescriptorPool* absl_nonnull,
      google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull) const>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<BinaryFunctionImpl>(std::move(fn));
  }

  template <typename F,
            typename = std::enable_if_t<std::is_invocable_v<F, U, V>>>
  static std::unique_ptr<cel::Function> WrapFunction(F&& function) {
    return WrapFunction(
        [function = std::forward<F>(function)](
            U arg1, V arg2, const google::protobuf::DescriptorPool* absl_nonnull,
            google::protobuf::MessageFactory* absl_nonnull,
            google::protobuf::Arena* absl_nonnull) -> T { return function(arg1, arg2); });
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict) {
    return CreateDescriptor(name, receiver_style,
                            {is_strict, /*is_contextual=*/false});
  }

  static FunctionDescriptor CreateDescriptor(
      absl::string_view name, bool receiver_style,
      FunctionDescriptorOptions options = {}) {
    return FunctionDescriptor(name, receiver_style,
                              {runtime_internal::AdaptedKind<U>(),
                               runtime_internal::AdaptedKind<V>()},
                              options);
  }

 private:
  class BinaryFunctionImpl : public cel::Function {
   public:
    explicit BinaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Value> Invoke(
        absl::Span<const Value> args,
        const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
        google::protobuf::MessageFactory* absl_nonnull message_factory,
        google::protobuf::Arena* absl_nonnull arena) const override {
      using Arg1Traits = runtime_internal::AdaptedTypeTraits<U>;
      using Arg2Traits = runtime_internal::AdaptedTypeTraits<V>;
      if (args.size() != 2) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for binary function");
      }
      typename Arg1Traits::AssignableType arg1;
      typename Arg2Traits::AssignableType arg2;
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[0]}(&arg1));
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[1]}(&arg2));

      if constexpr (std::is_same_v<T, Value> ||
                    std::is_same_v<T, absl::StatusOr<Value>>) {
        return fn_(Arg1Traits::ToArg(arg1), Arg2Traits::ToArg(arg2),
                   descriptor_pool, message_factory, arena);
      } else {
        T result = fn_(Arg1Traits::ToArg(arg1), Arg2Traits::ToArg(arg2),
                       descriptor_pool, message_factory, arena);

        return runtime_internal::AdaptedToValueVisitor{}(std::move(result));
      }
    }

   private:
    BinaryFunctionAdapter::FunctionType fn_;
  };
};

template <typename T, typename U, typename V, typename W>
class TernaryFunctionAdapter
    : public RegisterHelper<TernaryFunctionAdapter<T, U, V, W>> {
 public:
  using FunctionType = absl::AnyInvocable<T(
      U, V, W, const google::protobuf::DescriptorPool* absl_nonnull,
      google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull) const>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<TernaryFunctionImpl>(std::move(fn));
  }

  template <typename F,
            typename = std::enable_if_t<std::is_invocable_v<F, U, V, W>>>
  static std::unique_ptr<cel::Function> WrapFunction(F&& function) {
    return WrapFunction([function = std::forward<F>(function)](
                            U arg1, V arg2, W arg3,
                            const google::protobuf::DescriptorPool* absl_nonnull,
                            google::protobuf::MessageFactory* absl_nonnull,
                            google::protobuf::Arena* absl_nonnull) -> T {
      return function(arg1, arg2, arg3);
    });
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict) {
    return CreateDescriptor(
        name, receiver_style,
        FunctionDescriptorOptions{is_strict, /*is_contextual=*/false});
  }

  static FunctionDescriptor CreateDescriptor(
      absl::string_view name, bool receiver_style,
      FunctionDescriptorOptions options = {}) {
    return FunctionDescriptor(
        name, receiver_style,
        {runtime_internal::AdaptedKind<U>(), runtime_internal::AdaptedKind<V>(),
         runtime_internal::AdaptedKind<W>()},
        options);
  }

 private:
  class TernaryFunctionImpl : public cel::Function {
   public:
    explicit TernaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Value> Invoke(
        absl::Span<const Value> args,
        const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
        google::protobuf::MessageFactory* absl_nonnull message_factory,
        google::protobuf::Arena* absl_nonnull arena) const override {
      using Arg1Traits = runtime_internal::AdaptedTypeTraits<U>;
      using Arg2Traits = runtime_internal::AdaptedTypeTraits<V>;
      using Arg3Traits = runtime_internal::AdaptedTypeTraits<W>;
      if (args.size() != 3) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for ternary function");
      }
      typename Arg1Traits::AssignableType arg1;
      typename Arg2Traits::AssignableType arg2;
      typename Arg3Traits::AssignableType arg3;
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[0]}(&arg1));
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[1]}(&arg2));
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[2]}(&arg3));

      if constexpr (std::is_same_v<T, Value> ||
                    std::is_same_v<T, absl::StatusOr<Value>>) {
        return fn_(Arg1Traits::ToArg(arg1), Arg2Traits::ToArg(arg2),
                   Arg3Traits::ToArg(arg3), descriptor_pool, message_factory,
                   arena);
      } else {
        T result = fn_(Arg1Traits::ToArg(arg1), Arg2Traits::ToArg(arg2),
                       Arg3Traits::ToArg(arg3), descriptor_pool,
                       message_factory, arena);

        return runtime_internal::AdaptedToValueVisitor{}(std::move(result));
      }
    }

   private:
    TernaryFunctionAdapter::FunctionType fn_;
  };
};

template <typename T, typename U, typename V, typename W, typename X>
class QuaternaryFunctionAdapter
    : public RegisterHelper<QuaternaryFunctionAdapter<T, U, V, W, X>> {
 public:
  using FunctionType = absl::AnyInvocable<T(
      U, V, W, X, const google::protobuf::DescriptorPool* absl_nonnull,
      google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull) const>;

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<QuaternaryFunctionImpl>(std::move(fn));
  }

  template <typename F,
            typename = std::enable_if_t<std::is_invocable_v<F, U, V, W, X>>>
  static std::unique_ptr<cel::Function> WrapFunction(F&& function) {
    return WrapFunction([function = std::forward<F>(function)](
                            U arg1, V arg2, W arg3, X arg4,
                            const google::protobuf::DescriptorPool* absl_nonnull,
                            google::protobuf::MessageFactory* absl_nonnull,
                            google::protobuf::Arena* absl_nonnull) -> T {
      return function(arg1, arg2, arg3, arg4);
    });
  }

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict) {
    return CreateDescriptor(name, receiver_style,
                            {is_strict, /*is_contextual=*/false});
  }

  static FunctionDescriptor CreateDescriptor(
      absl::string_view name, bool receiver_style,
      FunctionDescriptorOptions options = {}) {
    return FunctionDescriptor(
        name, receiver_style,
        {runtime_internal::AdaptedKind<U>(), runtime_internal::AdaptedKind<V>(),
         runtime_internal::AdaptedKind<W>(),
         runtime_internal::AdaptedKind<X>()},
        options);
  }

 private:
  class QuaternaryFunctionImpl : public cel::Function {
   public:
    explicit QuaternaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Value> Invoke(
        absl::Span<const Value> args,
        const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
        google::protobuf::MessageFactory* absl_nonnull message_factory,
        google::protobuf::Arena* absl_nonnull arena) const override {
      using Arg1Traits = runtime_internal::AdaptedTypeTraits<U>;
      using Arg2Traits = runtime_internal::AdaptedTypeTraits<V>;
      using Arg3Traits = runtime_internal::AdaptedTypeTraits<W>;
      using Arg4Traits = runtime_internal::AdaptedTypeTraits<X>;
      if (args.size() != 4) {
        return absl::InvalidArgumentError(
            "unexpected number of arguments for quaternary function");
      }
      typename Arg1Traits::AssignableType arg1;
      typename Arg2Traits::AssignableType arg2;
      typename Arg3Traits::AssignableType arg3;
      typename Arg4Traits::AssignableType arg4;
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[0]}(&arg1));
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[1]}(&arg2));
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[2]}(&arg3));
      CEL_RETURN_IF_ERROR(
          runtime_internal::ValueToAdaptedVisitor{args[3]}(&arg4));

      if constexpr (std::is_same_v<T, Value> ||
                    std::is_same_v<T, absl::StatusOr<Value>>) {
        return fn_(Arg1Traits::ToArg(arg1), Arg2Traits::ToArg(arg2),
                   Arg3Traits::ToArg(arg3), Arg4Traits::ToArg(arg4),
                   descriptor_pool, message_factory, arena);
      } else {
        T result = fn_(Arg1Traits::ToArg(arg1), Arg2Traits::ToArg(arg2),
                       Arg3Traits::ToArg(arg3), Arg4Traits::ToArg(arg4),
                       descriptor_pool, message_factory, arena);

        return runtime_internal::AdaptedToValueVisitor{}(std::move(result));
      }
    }

   private:
    QuaternaryFunctionAdapter::FunctionType fn_;
  };
};

// Primary template for n-ary adapter.
template <typename T, typename... Args>
class NaryFunctionAdapter;

template <typename T>
class NaryFunctionAdapter<T> : public NullaryFunctionAdapter<T> {};

template <typename T, typename U>
class NaryFunctionAdapter<T, U> : public UnaryFunctionAdapter<T, U> {};

template <typename T, typename U, typename V>
class NaryFunctionAdapter<T, U, V> : public BinaryFunctionAdapter<T, U, V> {};

template <typename T, typename U, typename V, typename W>
class NaryFunctionAdapter<T, U, V, W>
    : public TernaryFunctionAdapter<T, U, V, W> {};

template <typename T, typename U, typename V, typename W, typename X>
class NaryFunctionAdapter<T, U, V, W, X>
    : public QuaternaryFunctionAdapter<T, U, V, W, X> {};

// N-ary function adapter.
//
// Prefer using one of the specific count adapters above for readability and
// better error messages.
template <typename T, typename... Args>
class NaryFunctionAdapter
    : public RegisterHelper<NaryFunctionAdapter<T, Args...>> {
 public:
  using FunctionType = absl::AnyInvocable<T(
      Args..., const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const>;

  static FunctionDescriptor CreateDescriptor(absl::string_view name,
                                             bool receiver_style,
                                             bool is_strict) {
    return CreateDescriptor(name, receiver_style,
                            {is_strict, /*is_contextual=*/false});
  }

  static FunctionDescriptor CreateDescriptor(
      absl::string_view name, bool receiver_style,
      FunctionDescriptorOptions options = {}) {
    return FunctionDescriptor(name, receiver_style,
                              {runtime_internal::AdaptedKind<Args>()...},
                              options);
  }

  static std::unique_ptr<cel::Function> WrapFunction(FunctionType fn) {
    return std::make_unique<NaryFunctionImpl>(std::move(fn));
  }

  template <typename F,
            typename = std::enable_if_t<std::is_invocable_v<F, Args...>>>
  static std::unique_ptr<cel::Function> WrapFunction(F&& function) {
    return WrapFunction(
        [function = std::forward<F>(function)](
            Args... args, const google::protobuf::DescriptorPool* absl_nonnull,
            google::protobuf::MessageFactory* absl_nonnull,
            google::protobuf::Arena* absl_nonnull) -> T { return function(args...); });
  }

 private:
  class NaryFunctionImpl : public cel::Function {
   private:
    using ArgBuffer = std::tuple<
        typename runtime_internal::AdaptedTypeTraits<Args>::AssignableType...>;

   public:
    explicit NaryFunctionImpl(FunctionType fn) : fn_(std::move(fn)) {}
    absl::StatusOr<Value> Invoke(
        absl::Span<const Value> args,
        const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
        google::protobuf::MessageFactory* absl_nonnull message_factory,
        google::protobuf::Arena* absl_nonnull arena) const override {
      if (args.size() != sizeof...(Args)) {
        return absl::InvalidArgumentError(
            absl::StrCat("unexpected number of arguments for ", sizeof...(Args),
                         "-ary function"));
      }
      ArgBuffer arg_buffer;
      CEL_RETURN_IF_ERROR(
          runtime_internal::AdaptHelper<Args...>::Apply(args, arg_buffer));
      if constexpr (std::is_same_v<T, Value> ||
                    std::is_same_v<T, absl::StatusOr<Value>>) {
        return runtime_internal::ToArgsHelper<Args...>::template Apply<T>(
            fn_, arg_buffer, descriptor_pool, message_factory, arena);
      } else {
        T result = runtime_internal::ToArgsHelper<Args...>::template Apply<T>(
            fn_, arg_buffer, descriptor_pool, message_factory, arena);
        return runtime_internal::AdaptedToValueVisitor{}(std::move(result));
      }
    }

   private:
    FunctionType fn_;
  };
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_FUNCTION_ADAPTER_H_
