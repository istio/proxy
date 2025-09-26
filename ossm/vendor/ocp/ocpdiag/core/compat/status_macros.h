// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_COMPAT_STATUS_MACROS_H_
#define OCPDIAG_CORE_COMPAT_STATUS_MACROS_H_

#include <filesystem>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

#define RETURN_IF_ERROR(s) \
  {                        \
    auto c = (s);          \
    if (!c.ok()) return c; \
  }

// `RETURN_IF_ERROR_WITH_MESSAGE` behave the same with RETURN_IF_ERROR in
// addition with a message.
//
// A message contains a key and a value. The Value is the message content.
// The key format is "ocpdiag/function name/filename#lineno.
//
// Example:
//
//   RETURN_IF_ERROR_WITH_MESSAGE(ParseConfigs(arg), "Failed to load config");
//
//   Returns a status: "INTERNAL: Failed to parse field.
//   [ocpdiag/LoadConfig/main.cc#25='Failed to load config.']"
#define RETURN_IF_ERROR_WITH_MESSAGE(s, msg)                                   \
  {                                                                            \
    auto c = (s);                                                              \
    if (!c.ok()) {                                                             \
      c.SetPayload(                                                            \
          absl::StrFormat("ocpdiag/%s/%s#%d", __func__,                         \
                          std::filesystem::path(__FILE__).filename().string(), \
                          __LINE__),                                           \
          absl::Cord(absl::string_view(msg)));                                 \
      return c;                                                                \
    }                                                                          \
  }

#define RETURN_VOID_IF_ERROR(s) \
  {                             \
    auto c = (s);               \
    if (!c.ok()) return;        \
  }

// Executes an expression `expr` that returns an `absl::StatusOr<T>`.
// On Ok, move its value into the variable defined by `var`,
// otherwise returns from the current function.
//
// Example: Declaring and initializing a new variable (ValueType can be anything
//          that can be initialized with assignment, including references):
//   ASSIGN_OR_RETURN(ValueType value, MaybeGetValue(arg));
//
// Example: Assigning to an existing variable:
//   ValueType value;
//   ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
#define ASSIGN_OR_RETURN(var, expr)           \
  OCPDIAG_STATUS_MACROS_ASSIGN_OR_RETURN_IMPL( \
      OCPDIAG_STATUS_MACROS_CONCAT(_status_or_expr, __LINE__), var, expr)

#define OCPDIAG_STATUS_MACROS_ASSIGN_OR_RETURN_IMPL(c, v, s) \
  auto c = (s);                                             \
  if (!c.ok()) return c.status();                           \
  v = std::move(c.value());

// Helpers for concatenating values, needed to construct "unique" name
#define OCPDIAG_STATUS_MACROS_CONCAT(x, y) \
  OCPDIAG_STATUS_MACROS_CONCAT_INNER(x, y)
#define OCPDIAG_STATUS_MACROS_CONCAT_INNER(x, y) x##y

// `ASSIGN_OR_RETURN_WITH_MESSAGE` behave the same with ASSIGN_OR_RETURN in
// addition with a message.
//
// A message contains a key and a value. The Value is the message content.
// The key format is "ocpdiag/function name/filename#lineno.
//
// Example:
//
//   ASSIGN_OR_RETURN_WITH_MESSAGE(value, ParseConfigs(arg),
//                                 "Failed to load config");
//
//   Returns a status: "INTERNAL: Failed to parse field.
//   [ocpdiag/LoadConfig/main.cc#25='Failed to load config.']"
#define ASSIGN_OR_RETURN_WITH_MESSAGE(var, expr, msg)       \
  OCPDIAG_STATUS_MACROS_ASSIGN_OR_RETURN__WITH_MESSAGE_IMPL( \
      OCPDIAG_STATUS_MACROS_CONCAT(_status_or_expr, __LINE__), var, expr, msg)

#define OCPDIAG_STATUS_MACROS_ASSIGN_OR_RETURN__WITH_MESSAGE_IMPL(c, v, s, msg) \
  auto c = (s);                                                                \
  if (!c.ok()) {                                                               \
    absl::Status r(c.status());                                                \
    r.SetPayload(                                                              \
        absl::StrFormat("ocpdiag/%s/%s#%d", __func__,                           \
                        std::filesystem::path(__FILE__).filename().string(),   \
                        __LINE__),                                             \
        absl::Cord(absl::string_view(msg)));                                   \
    return r;                                                                  \
  }                                                                            \
  v = std::move(c.value());

#endif  // OCPDIAG_CORE_COMPAT_STATUS_MACROS_H_
