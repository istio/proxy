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
#include "common/ast/navigable_ast_kinds.h"

#include <string>

#include "absl/strings/str_cat.h"

namespace cel {

std::string ChildKindName(ChildKind kind) {
  switch (kind) {
    case ChildKind::kUnspecified:
      return "Unspecified";
    case ChildKind::kSelectOperand:
      return "SelectOperand";
    case ChildKind::kCallReceiver:
      return "CallReceiver";
    case ChildKind::kCallArg:
      return "CallArg";
    case ChildKind::kListElem:
      return "ListElem";
    case ChildKind::kMapKey:
      return "MapKey";
    case ChildKind::kMapValue:
      return "MapValue";
    case ChildKind::kStructValue:
      return "StructValue";
    case ChildKind::kComprehensionRange:
      return "ComprehensionRange";
    case ChildKind::kComprehensionInit:
      return "ComprehensionInit";
    case ChildKind::kComprehensionCondition:
      return "ComprehensionCondition";
    case ChildKind::kComprehensionLoopStep:
      return "ComprehensionLoopStep";
    case ChildKind::kComprensionResult:
      return "ComprehensionResult";
    default:
      return absl::StrCat("Unknown ChildKind ", static_cast<int>(kind));
  }
}

std::string NodeKindName(NodeKind kind) {
  switch (kind) {
    case NodeKind::kUnspecified:
      return "Unspecified";
    case NodeKind::kConstant:
      return "Constant";
    case NodeKind::kIdent:
      return "Ident";
    case NodeKind::kSelect:
      return "Select";
    case NodeKind::kCall:
      return "Call";
    case NodeKind::kList:
      return "List";
    case NodeKind::kMap:
      return "Map";
    case NodeKind::kStruct:
      return "Struct";
    case NodeKind::kComprehension:
      return "Comprehension";
    default:
      return absl::StrCat("Unknown NodeKind ", static_cast<int>(kind));
  }
}

}  // namespace cel
