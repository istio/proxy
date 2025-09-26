// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_CEL_FIELD_EXTRACTOR_H
#define THIRD_PARTY_CEL_CPP_TOOLS_CEL_FIELD_EXTRACTOR_H

#include <string>

#include "cel/expr/syntax.pb.h"
#include "absl/container/flat_hash_set.h"

namespace cel {

//   ExtractExpressionFieldPaths attempts to extract the set of unique field
//   selection paths from top level identifiers (e.g. "request.user.id").
//
//   One possible use case for this class is to determine which fields of a
//   serialized message are referenced by a CEL query, enabling partial
//   deserialization for performance optimization.
//
//   Implementation notes:
//   The extraction logic focuses on identifying chains of `Select` operations
//   that terminate with a primary identifier node (`IdentExpr`). For example,
//   in the expression `message.field.subfield == 10`, the path
//   "message.field.subfield" would be extracted.
//
//   Identifiers defined locally within CEL comprehension expressions (e.g.,
//   comprehension variables aliases defined by `iter_var`, `iter_var2`,
//   `accu_var` in the AST) are NOT included. Example:
//   `list.exists(elem, elem.field == 'value')` would return {"list"} only.
//
//   Container indexing with the _[_] is not considered, but map indexing with
//   the select operator is considered. For example:
//   `message.map_field.key || message.map_field['foo']` results in
//   {'message.map_field.key', 'message.map_field'}
//
//   This implementation does not consider type check metadata, so there is no
//   understanding of whether the primary identifiers and field accesses
//   necessarily map to proto messages or proto field accesses. The field
//   also does not have any understanding of the type of the leaf of the
//   select path.
//
//   Example:
//   Given the CEL expression:
//   `(request.user.id == 'test' && request.user.attributes.exists(attr,
//   attr.key
//   == 'role')) || size(request.items) > 0`
//
//   The extracted field paths would be:
//   - "request.user.id"
//   - "request.user.attributes" (because `attr` is a comprehension variable)
//   - "request.items"

absl::flat_hash_set<std::string> ExtractFieldPaths(
    const cel::expr::Expr& expr);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_CEL_FIELD_EXTRACTOR_H
