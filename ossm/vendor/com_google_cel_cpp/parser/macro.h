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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_
#define THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/expr.h"
#include "parser/macro_expr_factory.h"

namespace cel {

// MacroExpander converts the arguments of a function call that matches a
// Macro.
//
// If this is a receiver-style macro, the second argument (optional expr) will
// be engaged. In the case of a global call, it will be `absl::nullopt`.
//
// Should return the replacement subexpression if replacement should occur,
// otherwise absl::nullopt. If `absl::nullopt` is returned, none of the
// arguments including the target must have been modified. Doing so is undefined
// behavior. Otherwise the expander is free to mutate the arguments and either
// include or exclude them from the result.
//
// We use `std::reference_wrapper<Expr>` to be consistent with the fact that we
// do not use raw pointers elsewhere with `Expr` and friends. Ideally we would
// just use `absl::optional<Expr&>`, but that is not currently allowed and our
// `optional_ref<T>` is internal.
using MacroExpander = absl::AnyInvocable<absl::optional<Expr>(
    MacroExprFactory&, absl::optional<std::reference_wrapper<Expr>>,
    absl::Span<Expr>) const>;

// `GlobalMacroExpander` is a `MacroExpander` for global macros.
using GlobalMacroExpander = absl::AnyInvocable<absl::optional<Expr>(
    MacroExprFactory&, absl::Span<Expr>) const>;

// `ReceiverMacroExpander` is a `MacroExpander` for receiver-style macros.
using ReceiverMacroExpander = absl::AnyInvocable<absl::optional<Expr>(
    MacroExprFactory&, Expr&, absl::Span<Expr>) const>;

// Macro interface for describing the function signature to match and the
// MacroExpander to apply.
//
// Note: when a Macro should apply to multiple overloads (based on arg count) of
// a given function, a Macro should be created per arg-count.
class Macro final {
 public:
  static absl::StatusOr<Macro> Global(absl::string_view name,
                                      size_t argument_count,
                                      GlobalMacroExpander expander);

  static absl::StatusOr<Macro> GlobalVarArg(absl::string_view name,
                                            GlobalMacroExpander expander);

  static absl::StatusOr<Macro> Receiver(absl::string_view name,
                                        size_t argument_count,
                                        ReceiverMacroExpander expander);

  static absl::StatusOr<Macro> ReceiverVarArg(absl::string_view name,
                                              ReceiverMacroExpander expander);

  Macro(const Macro&) = default;
  Macro(Macro&&) = default;
  Macro& operator=(const Macro&) = default;
  Macro& operator=(Macro&&) = default;

  // Function name to match.
  absl::string_view function() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return rep_->function;
  }

  // argument_count() for the function call.
  //
  // When the macro is a var-arg style macro, the return value will be zero, but
  // the MacroKey will contain a `*` where the arg count would have been.
  size_t argument_count() const { return rep_->arg_count; }

  // is_receiver_style returns true if the macro matches a receiver style call.
  bool is_receiver_style() const { return rep_->receiver_style; }

  bool is_variadic() const { return rep_->var_arg_style; }

  // key() returns the macro signatures accepted by this macro.
  //
  // Format: `<function>:<arg-count>:<is-receiver>`.
  //
  // When the macros is a var-arg style macro, the `arg-count` value is
  // represented as a `*`.
  absl::string_view key() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return rep_->key;
  }

  // Expander returns the MacroExpander to apply when the macro key matches the
  // parsed call signature.
  const MacroExpander& expander() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return rep_->expander;
  }

  ABSL_MUST_USE_RESULT absl::optional<Expr> Expand(
      MacroExprFactory& factory,
      absl::optional<std::reference_wrapper<Expr>> target,
      absl::Span<Expr> arguments) const {
    return (expander())(factory, target, arguments);
  }

  friend void swap(Macro& lhs, Macro& rhs) noexcept {
    using std::swap;
    swap(lhs.rep_, rhs.rep_);
  }

  ABSL_DEPRECATED("use MacroRegistry and RegisterStandardMacros")
  static std::vector<Macro> AllMacros();

 private:
  struct Rep final {
    Rep(std::string function, std::string key, size_t arg_count,
        MacroExpander expander, bool receiver_style, bool var_arg_style)
        : function(std::move(function)),
          key(std::move(key)),
          arg_count(arg_count),
          expander(std::move(expander)),
          receiver_style(receiver_style),
          var_arg_style(var_arg_style) {}

    std::string function;
    std::string key;
    size_t arg_count;
    MacroExpander expander;
    bool receiver_style;
    bool var_arg_style;
  };

  static std::string Key(absl::string_view name, size_t argument_count,
                         bool receiver_style, bool var_arg_style);

  static absl::StatusOr<Macro> Make(absl::string_view name,
                                    size_t argument_count,
                                    MacroExpander expander, bool receiver_style,
                                    bool var_arg_style);

  explicit Macro(std::shared_ptr<const Rep> rep) : rep_(std::move(rep)) {}

  std::shared_ptr<const Rep> rep_;
};

// The macro "has(m.f)" which tests the presence of a field, avoiding the
// need to specify the field as a string.
const Macro& HasMacro();

// The macro "range.all(var, predicate)", which is true if for all
// elements in range the predicate holds.
const Macro& AllMacro();

// The macro "range.exists(var, predicate)", which is true if for at least
// one element in range the predicate holds.
const Macro& ExistsMacro();

// The macro "range.exists_one(var, predicate)", which is true if for
// exactly one element in range the predicate holds.
const Macro& ExistsOneMacro();

// The macro "range.map(var, function)", applies the function to the vars
// in the range.
const Macro& Map2Macro();

// The macro "range.map(var, predicate, function)", applies the function
// to the vars in the range for which the predicate holds true. The other
// variables are filtered out.
const Macro& Map3Macro();

// The macro "range.filter(var, predicate)", filters out the variables for
// which the predicate is false.
const Macro& FilterMacro();

// `OptMapMacro`
//
// Apply a transformation to the optional's underlying value if it is not empty
// and return an optional typed result based on the transformation. The
// transformation expression type must return a type T which is wrapped into
// an optional.
//
//  msg.?elements.optMap(e, e.size()).orValue(0)
const Macro& OptMapMacro();

// `OptFlatMapMacro`
//
// Apply a transformation to the optional's underlying value if it is not empty
// and return the result. The transform expression must return an optional(T)
// rather than type T. This can be useful when dealing with zero values and
// conditionally generating an empty or non-empty result in ways which cannot
// be expressed with `optMap`.
//
//  msg.?elements.optFlatMap(e, e[?0]) // return the first element if present.
const Macro& OptFlatMapMacro();

}  // namespace cel

namespace google::api::expr::parser {

using MacroExpander = cel::MacroExpander;

using Macro = cel::Macro;

}  // namespace google::api::expr::parser

#endif  // THIRD_PARTY_CEL_CPP_PARSER_MACRO_H_
