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
// IWYU pragma: private
#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_KINDS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_KINDS_H_

#include <string>

#include "absl/strings/str_format.h"

namespace cel {

// The traversal relationship from parent to the given node in a NavigableAst.
enum class ChildKind {
  kUnspecified,
  kSelectOperand,
  kCallReceiver,
  kCallArg,
  kListElem,
  kMapKey,
  kMapValue,
  kStructValue,
  kComprehensionRange,
  kComprehensionInit,
  kComprehensionCondition,
  kComprehensionLoopStep,
  kComprensionResult
};

// The type of the node in a NavigableAst.
enum class NodeKind {
  kUnspecified,
  kConstant,
  kIdent,
  kSelect,
  kCall,
  kList,
  kMap,
  kStruct,
  kComprehension,
};

// Human readable ChildKind name. Provided for test readability -- do not depend
// on the specific values.
std::string ChildKindName(ChildKind kind);

template <typename Sink>
void AbslStringify(Sink& sink, ChildKind kind) {
  absl::Format(&sink, "%s", ChildKindName(kind));
}

// Human readable NodeKind name. Provided for test readability -- do not depend
// on the specific values.
std::string NodeKindName(NodeKind kind);

template <typename Sink>
void AbslStringify(Sink& sink, NodeKind kind) {
  absl::Format(&sink, "%s", NodeKindName(kind));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_KINDS_H_
