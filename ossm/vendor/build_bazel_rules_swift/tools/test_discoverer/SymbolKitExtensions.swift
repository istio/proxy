// Copyright 2022 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import SymbolKit

extension SymbolGraph.Symbol {
  /// Returns true if the given symbol represents an `async` declaration, or false otherwise.
  var isAsyncDeclaration: Bool {
    guard let mixin = declarationFragments else { return false }

    return mixin.declarationFragments.contains { fragment in
      fragment.kind == .keyword && fragment.spelling == "async"
    }
  }

  /// Returns the symbol's `DeclarationFragments` mixin, or `nil` if it does not exist.
  var declarationFragments: DeclarationFragments? {
    mixins[DeclarationFragments.mixinKey] as? DeclarationFragments
  }

  /// Returns the symbol's `FunctionSignature` mixin, or `nil` if it does not exist.
  var functionSignature: FunctionSignature? {
    mixins[FunctionSignature.mixinKey] as? FunctionSignature
  }

  /// Returns the symbol's `Swift.Generics` mixin, or `nil` if it does not exist.
  var swiftGenerics: Swift.Generics? {
    mixins[Swift.Generics.mixinKey] as? Swift.Generics
  }
}

extension SymbolGraph.Symbol.FunctionSignature {
  /// Returns true if the given function signature satisfies the requirements to be a test function;
  /// that is, it has no parameters and returns `Void`.
  var isTestLike: Bool {
    // TODO(b/220940013): Do we need to support the `Void` spelling here too, if someone writes
    // `Void` specifically instead of omitting the return type?
    parameters.isEmpty
      && returns.count == 1
      && returns[0].kind == .text
      && returns[0].spelling == "()"
  }
}
