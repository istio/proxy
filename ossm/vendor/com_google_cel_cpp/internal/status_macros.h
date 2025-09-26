// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_MACROS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_MACROS_H_

#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "internal/status_builder.h"

#define CEL_RETURN_IF_ERROR(expr)                                            \
  CEL_INTERNAL_STATUS_MACROS_IMPL_ELSE_BLOCKER_                              \
  if (::cel::internal::StatusAdaptor cel_internal_status_macro = {(expr)}) { \
  } else /* NOLINT */                                                        \
    return cel_internal_status_macro.Consume()

// The GNU compiler historically emitted warnings for obscure usages of
// `if (foo) if (bar) {} else`. This suppresses that.

// clang-format off
#define CEL_INTERNAL_STATUS_MACROS_IMPL_ELSE_BLOCKER_ \
  switch (0) case 0: default: /* NOLINT */
// clang-format on

#define CEL_ASSIGN_OR_RETURN(...)                                   \
  CEL_INTERNAL_STATUS_MACROS_GET_VARIADIC_(                         \
      (__VA_ARGS__, CEL_INTERNAL_STATUS_MACROS_ASSIGN_OR_RETURN_3_, \
       CEL_INTERNAL_STATUS_MACROS_ASSIGN_OR_RETURN_2_))             \
  (__VA_ARGS__)

// The following are macro magic to select either the 2 arg variant or 3 arg
// variant of CEL_ASSIGN_OR_RETURN.

#define CEL_INTERNAL_STATUS_MACROS_GET_VARIADIC_HELPER_(_1, _2, _3, NAME, ...) \
  NAME
#define CEL_INTERNAL_STATUS_MACROS_GET_VARIADIC_(args) \
  CEL_INTERNAL_STATUS_MACROS_GET_VARIADIC_HELPER_ args

#define CEL_INTERNAL_STATUS_MACROS_ASSIGN_OR_RETURN_2_(lhs, rexpr)        \
  CEL_INTERNAL_STATUS_MACROS_ASSIGN_OR_RETURN_(                           \
      CEL_INTERNAL_STATUS_MACROS_CONCAT(_status_or_value, __LINE__), lhs, \
      rexpr,                                                              \
      return absl::Status(std::move(CEL_INTERNAL_STATUS_MACROS_CONCAT(    \
                                        _status_or_value, __LINE__))      \
                              .status()))

#define CEL_INTERNAL_STATUS_MACROS_ASSIGN_OR_RETURN_3_(lhs, rexpr,           \
                                                       error_expression)     \
  CEL_INTERNAL_STATUS_MACROS_ASSIGN_OR_RETURN_(                              \
      CEL_INTERNAL_STATUS_MACROS_CONCAT(_status_or_value, __LINE__), lhs,    \
      rexpr,                                                                 \
      ::cel::internal::StatusBuilder _(                                      \
          std::move(                                                         \
              CEL_INTERNAL_STATUS_MACROS_CONCAT(_status_or_value, __LINE__)) \
              .status());                                                    \
      (void)_; /* error_expression is allowed to not use this variable */    \
      return (error_expression))

// Common implementation of CEL_ASSIGN_OR_RETURN. Both the 2 arg variant and 3
// arg variant are implemented by this macro.

#define CEL_INTERNAL_STATUS_MACROS_ASSIGN_OR_RETURN_(statusor, lhs, rexpr, \
                                                     error_expression)     \
  auto statusor = (rexpr);                                                 \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {                                \
    error_expression;                                                      \
  }                                                                        \
  CEL_INTERNAL_STATUS_MACROS_UNPARENTHESIZE_IF_PARENTHESIZED(lhs) =        \
      std::move(statusor).value()

#define CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_INNER(...) \
  CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_INNER_HELPER((__VA_ARGS__, 0, 1))

// MSVC historically expands variadic macros incorrectly, so another level of
// indirection is required.
#define CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_INNER_HELPER(args) \
  CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_INNER_I args
#define CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_INNER_I(e0, e1, is_empty, ...) \
  is_empty

#define CEL_INTERNAL_STATUS_MACROS_IS_EMPTY(...) \
  CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_I(__VA_ARGS__)
#define CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_I(...) \
  CEL_INTERNAL_STATUS_MACROS_IS_EMPTY_INNER(_, ##__VA_ARGS__)

#define CEL_INTERNAL_STATUS_MACROS_IF_1(_Then, _Else) _Then
#define CEL_INTERNAL_STATUS_MACROS_IF_0(_Then, _Else) _Else
#define CEL_INTERNAL_STATUS_MACROS_IF(_Cond, _Then, _Else)                 \
  CEL_INTERNAL_STATUS_MACROS_CONCAT(CEL_INTERNAL_STATUS_MACROS_IF_, _Cond) \
  (_Then, _Else)

#define CEL_INTERNAL_STATUS_MACROS_EAT(...)
#define CEL_INTERNAL_STATUS_MACROS_REM(...) __VA_ARGS__
#define CEL_INTERNAL_STATUS_MACROS_EMPTY()

// Expands to 1 if the input is surrounded by parenthesis, 0 otherwise.
#define CEL_INTERNAL_STATUS_MACROS_IS_PARENTHESIZED(...) \
  CEL_INTERNAL_STATUS_MACROS_IS_EMPTY(                   \
      CEL_INTERNAL_STATUS_MACROS_EAT __VA_ARGS__)

// If the input is surrounded by parenthesis, remove them. Otherwise expand it
// unchanged.
#define CEL_INTERNAL_STATUS_MACROS_UNPARENTHESIZE_IF_PARENTHESIZED(...)   \
  CEL_INTERNAL_STATUS_MACROS_IF(                                          \
      CEL_INTERNAL_STATUS_MACROS_IS_PARENTHESIZED(__VA_ARGS__),           \
      CEL_INTERNAL_STATUS_MACROS_REM, CEL_INTERNAL_STATUS_MACROS_EMPTY()) \
  __VA_ARGS__

#define CEL_INTERNAL_STATUS_MACROS_CONCAT_HELPER(x, y) x##y
#define CEL_INTERNAL_STATUS_MACROS_CONCAT(x, y) \
  CEL_INTERNAL_STATUS_MACROS_CONCAT_HELPER(x, y)

namespace cel::internal {

class StatusAdaptor final {
 public:
  StatusAdaptor() = default;

  StatusAdaptor(const StatusAdaptor&) = delete;

  StatusAdaptor(StatusAdaptor&&) = delete;

  StatusAdaptor(const absl::Status& status) : builder_(status) {}  // NOLINT

  StatusAdaptor& operator=(const StatusAdaptor&) = delete;

  StatusAdaptor& operator=(StatusAdaptor&&) = delete;

  StatusBuilder&& Consume() { return std::move(builder_); }

  explicit operator bool() const { return ABSL_PREDICT_TRUE(builder_.ok()); }

 private:
  StatusBuilder builder_;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_MACROS_H_
