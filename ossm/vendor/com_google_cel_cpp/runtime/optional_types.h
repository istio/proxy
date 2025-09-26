// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_OPTIONAL_TYPES_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_OPTIONAL_TYPES_H_

#include "absl/status/status.h"
#include "runtime/runtime_builder.h"

namespace cel::extensions {

// EnableOptionalTypes enable support for optional syntax and types in CEL.
//
// The optional value type makes it possible to express whether variables have
// been provided, whether a result has been computed, and in the future whether
// an object field path, map key value, or list index has a value.
//
// # Syntax Changes
//
// OptionalTypes are unlike other CEL extensions because they modify the CEL
// syntax itself, notably through the use of a `?` preceding a field name or
// index value.
//
// ## Field Selection
//
// The optional syntax in field selection is denoted as `obj.?field`. In other
// words, if a field is set, return `optional.of(obj.field)“, else
// `optional.none()`. The optional field selection is viral in the sense that
// after the first optional selection all subsequent selections or indices
// are treated as optional, i.e. the following expressions are equivalent:
//
//  obj.?field.subfield
//  obj.?field.?subfield
//
// ## Indexing
//
// Similar to field selection, the optional syntax can be used in index
// expressions on maps and lists:
//
//  list[?0]
//  map[?key]
//
// ## Optional Field Setting
//
// When creating map or message literals, if a field may be optionally set
// based on its presence, then placing a `?` before the field name or key
// will ensure the type on the right-hand side must be optional(T) where T
// is the type of the field or key-value.
//
// The following returns a map with the key expression set only if the
// subfield is present, otherwise an empty map is created:
//
//  {?key: obj.?field.subfield}
//
// ## Optional Element Setting
//
// When creating list literals, an element in the list may be optionally added
// when the element expression is preceded by a `?`:
//
//  [a, ?b, ?c] // return a list with either [a], [a, b], [a, b, c], or [a, c]
//
// # Optional.Of
//
// Create an optional(T) value of a given value with type T.
//
//  optional.of(10)
//
// # Optional.OfNonZeroValue
//
// Create an optional(T) value of a given value with type T if it is not a
// zero-value. A zero-value the default empty value for any given CEL type,
// including empty protobuf message types. If the value is empty, the result
// of this call will be optional.none().
//
//  optional.ofNonZeroValue([1, 2, 3]) // optional(list(int))
//  optional.ofNonZeroValue([]) // optional.none()
//  optional.ofNonZeroValue(0)  // optional.none()
//  optional.ofNonZeroValue("") // optional.none()
//
// # Optional.None
//
// Create an empty optional value.
//
// # HasValue
//
// Determine whether the optional contains a value.
//
//  optional.of(b'hello').hasValue() // true
//  optional.ofNonZeroValue({}).hasValue() // false
//
// # Value
//
// Get the value contained by the optional. If the optional does not have a
// value, the result will be a CEL error.
//
//  optional.of(b'hello').value() // b'hello'
//  optional.ofNonZeroValue({}).value() // error
//
// # Or
//
// If the value on the left-hand side is optional.none(), the optional value
// on the right hand side is returned. If the value on the left-hand set is
// valued, then it is returned. This operation is short-circuiting and will
// only evaluate as many links in the `or` chain as are needed to return a
// non-empty optional value.
//
//  obj.?field.or(m[?key])
//  l[?index].or(obj.?field.subfield).or(obj.?other)
//
// # OrValue
//
// Either return the value contained within the optional on the left-hand side
// or return the alternative value on the right hand side.
//
//  m[?key].orValue("none")
//
// # OptMap
//
// Apply a transformation to the optional's underlying value if it is not empty
// and return an optional typed result based on the transformation. The
// transformation expression type must return a type T which is wrapped into
// an optional.
//
//  msg.?elements.optMap(e, e.size()).orValue(0)
//
// # OptFlatMap
//
// Introduced in version: 1
//
// Apply a transformation to the optional's underlying value if it is not empty
// and return the result. The transform expression must return an optional(T)
// rather than type T. This can be useful when dealing with zero values and
// conditionally generating an empty or non-empty result in ways which cannot
// be expressed with `optMap`.
//
//  msg.?elements.optFlatMap(e, e[?0]) // return the first element if present.
absl::Status EnableOptionalTypes(RuntimeBuilder& builder);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_OPTIONAL_TYPES_H_
