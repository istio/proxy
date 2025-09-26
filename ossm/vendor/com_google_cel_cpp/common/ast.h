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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_H_

#include "common/expr.h"

namespace cel {

namespace ast_internal {
// Forward declare supported implementations.
class AstImpl;
}  // namespace ast_internal

// Runtime representation of a CEL expression's Abstract Syntax Tree.
//
// This class provides public APIs for CEL users and allows for clients to
// manage lifecycle.
//
// Implementations are intentionally opaque to prevent dependencies on the
// details of the runtime representation. To create a new instance, from a
// protobuf representation, use the conversion utilities in
// `extensions/protobuf/ast_converters.h`.
class Ast {
 public:
  virtual ~Ast() = default;

  // Whether the AST includes type check information.
  // If false, the runtime assumes all types are dyn, and that qualified names
  // have not been resolved.
  virtual bool IsChecked() const = 0;

 private:
  // This interface should only be implemented by friend-visibility allowed
  // subclasses.
  Ast() = default;
  friend class ast_internal::AstImpl;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_H_
