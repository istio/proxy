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

#ifndef THIRD_PARTY_CEL_CPP_BASE_BUILTINS_H_
#define THIRD_PARTY_CEL_CPP_BASE_BUILTINS_H_

namespace cel {

// Constants specifying names for CEL builtins.
//
// Prefer to use the constants in `common/standard_definitions.h`.
namespace builtin {

// Comparison
constexpr char kEqual[] = "_==_";
constexpr char kInequal[] = "_!=_";
constexpr char kLess[] = "_<_";
constexpr char kLessOrEqual[] = "_<=_";
constexpr char kGreater[] = "_>_";
constexpr char kGreaterOrEqual[] = "_>=_";

// Logical
constexpr char kAnd[] = "_&&_";
constexpr char kOr[] = "_||_";
constexpr char kNot[] = "!_";

// Strictness
constexpr char kNotStrictlyFalse[] = "@not_strictly_false";
// Deprecated '__not_strictly_false__' function. Preserved for backwards
// compatibility with stored expressions.
constexpr char kNotStrictlyFalseDeprecated[] = "__not_strictly_false__";

// Arithmetical
constexpr char kAdd[] = "_+_";
constexpr char kSubtract[] = "_-_";
constexpr char kNeg[] = "-_";
constexpr char kMultiply[] = "_*_";
constexpr char kDivide[] = "_/_";
constexpr char kModulo[] = "_%_";

// String operations
constexpr char kRegexMatch[] = "matches";
constexpr char kStringContains[] = "contains";
constexpr char kStringEndsWith[] = "endsWith";
constexpr char kStringStartsWith[] = "startsWith";

// Container operations
constexpr char kIn[] = "@in";
// Deprecated '_in_' operator. Preserved for backwards compatibility with stored
// expressions.
constexpr char kInDeprecated[] = "_in_";
// Deprecated 'in()' function. Preserved for backwards compatibility with stored
// expressions.
constexpr char kInFunction[] = "in";
constexpr char kIndex[] = "_[_]";
constexpr char kSize[] = "size";

constexpr char kTernary[] = "_?_:_";

// Timestamp and Duration
constexpr char kDuration[] = "duration";
constexpr char kTimestamp[] = "timestamp";
constexpr char kFullYear[] = "getFullYear";
constexpr char kMonth[] = "getMonth";
constexpr char kDayOfYear[] = "getDayOfYear";
constexpr char kDayOfMonth[] = "getDayOfMonth";
constexpr char kDate[] = "getDate";
constexpr char kDayOfWeek[] = "getDayOfWeek";
constexpr char kHours[] = "getHours";
constexpr char kMinutes[] = "getMinutes";
constexpr char kSeconds[] = "getSeconds";
constexpr char kMilliseconds[] = "getMilliseconds";

// Type conversions
constexpr char kBool[] = "bool";
constexpr char kBytes[] = "bytes";
constexpr char kDouble[] = "double";
constexpr char kDyn[] = "dyn";
constexpr char kInt[] = "int";
constexpr char kString[] = "string";
constexpr char kType[] = "type";
constexpr char kUint[] = "uint";

// Runtime-only functions.
// The convention for runtime-only functions where only the runtime needs to
// differentiate behavior is to prefix the function with `#`.
// Note, this is a different convention from CEL internal functions where the
// whole stack needs to be aware of the function id.
constexpr char kRuntimeListAppend[] = "#list_append";

}  // namespace builtin

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_BUILTINS_H_
