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

/// The precise identifier of the `XCTest.XCTestCase` class.
///
/// This is the mangled name of `XCTest.XCTestCase` with the leading "$s" replaced by "s:" (a common
/// notation used in Clang/Swift UIDs used for indexing).
private let xcTestCasePreciseIdentifier = "s:6XCTest0A4CaseC"

/// Collects information from one or more symbol graphs in order to determine which classes and
/// methods correspond to `XCTest`-style test cases.
final class SymbolCollector {
  /// `inheritsFrom` relationships collected from symbol graphs, keyed by their source identifier
  /// (i.e., the subclass in the relationship).
  private var inheritanceRelationships: [String: SymbolGraph.Relationship] = [:]

  /// `memberOf` relationships collected from symbol graphs, keyed by their source identifier (i.e.,
  /// the nested or contained declaration).
  private var memberRelationships: [String: SymbolGraph.Relationship] = [:]

  /// A mapping from class identifiers to Boolean values indicating whether or not the class is
  /// known to be or not be a test class (that is, inherit directly or indirectly from
  /// `XCTestCase`).
  ///
  /// If the value for a class identifier is true, the class is known to inherit from `XCTestCase`.
  /// If the value is false, the class is known to not inherit from `XCTestCase`. If the class
  /// identifier is not present in the map, then its state is not yet known.
  private var testCaseClassCache: [String: Bool] = [:]

  /// A mapping from discovered class identifiers to their symbol graph data.
  private var possibleTestClasses: [String: SymbolGraph.Symbol] = [:]

  /// A collection of methods that match heuristics to be considered as test methods -- their names
  /// begin with "test", they take no arguments, and they return `Void`.
  private var possibleTestMethods: [SymbolGraph.Symbol] = []

  /// A mapping from class (or class extension) identifiers to the module name where they were
  /// declared.
  private var modulesForClassIdentifiers: [String: String] = [:]

  /// Collects information from the given symbol graph that is needed to discover test classes and
  /// test methods in the module.
  func consume(_ symbolGraph: SymbolGraph) {
    // First, collect all the inheritance and member relationships from the graph. We cannot filter
    // them at this time, since they only contain the identifiers and might reference symbols in
    // modules whose graphs haven't been processed yet.
    for relationship in symbolGraph.relationships {
      switch relationship.kind {
      case .inheritsFrom:
        inheritanceRelationships[relationship.source] = relationship
      case .memberOf:
        memberRelationships[relationship.source] = relationship
      default:
        break
      }
    }

    // Next, collect classes and methods that might be tests. We can do limited filtering here, as
    // described below.
    symbolLoop: for (preciseIdentifier, symbol) in symbolGraph.symbols {
      switch symbol.kind.identifier {
      case .class:
        // Keep track of all classes for now; their inheritance relationships will be resolved
        // on-demand once we have all the symbol graphs loaded.
        possibleTestClasses[preciseIdentifier] = symbol
        modulesForClassIdentifiers[preciseIdentifier] = symbolGraph.module.name

      case .method:
        // Swift Package Manager uses the index store to discover tests; index-while-building writes
        // a unit-test property for any method that satisfies this method:
        // https://github.com/apple/swift/blob/da3856c45b7149730d6e5fdf528ac82b43daccac/lib/Index/IndexSymbol.cpp#L40-L82
        // We duplicate that logic here.

        guard symbol.swiftGenerics == nil else {
          // Generic methods cannot be tests.
          continue symbolLoop
        }

        guard symbol.functionSignature?.isTestLike == true else {
          // Functions with parameters or which return something other than `Void` cannot be tests.
          continue symbolLoop
        }

        let lastComponent = symbol.pathComponents.last!
        guard lastComponent.hasPrefix("test") else {
          // Test methods must be named `test*`.
          continue symbolLoop
        }

        // If we got this far, record the symbol as a possible test method. We still need to make
        // sure later that it is a member of a class that inherits from `XCTestCase`.
        possibleTestMethods.append(symbol)

      default:
        break
      }
    }
  }

  /// Returns a `DiscoveredTests` value containing structured information about the tests discovered
  /// in the symbol graph.
  func discoveredTests() -> DiscoveredTests {
    var discoveredTests = DiscoveredTests()

    for method in possibleTestMethods {
      if let classSymbol = testClassSymbol(for: method),
        let moduleName = modulesForClassIdentifiers[classSymbol.identifier.precise]
      {
        let className = classSymbol.pathComponents.last!

        let lastMethodComponent = method.pathComponents.last!
        let methodName =
          lastMethodComponent.hasSuffix("()")
          ? String(lastMethodComponent.dropLast(2))
          : lastMethodComponent

        discoveredTests.modules[moduleName, default: DiscoveredTests.Module(name: moduleName)]
          .classes[className, default: DiscoveredTests.Class(name: className)]
          .methods.append(
            DiscoveredTests.Method(name: methodName, isAsync: method.isAsyncDeclaration))
      }
    }

    return discoveredTests
  }
}

extension SymbolCollector {
  /// Returns the symbol graph symbol information for the class (or class extension) that contains
  /// the given method if and only if the class or class extension is a test class.
  ///
  /// If the containing class is unknown or it is not a test class, this method returns nil.
  private func testClassSymbol(for method: SymbolGraph.Symbol) -> SymbolGraph.Symbol? {
    guard let memberRelationship = memberRelationships[method.identifier.precise] else {
      return nil
    }

    let classIdentifier = memberRelationship.target
    guard isTestClass(classIdentifier) else {
      return nil
    }

    return possibleTestClasses[classIdentifier]
  }

  /// Returns a value indicating whether or not the class with the given identifier extends
  /// `XCTestCase` (or if the identifier is a class extension, whether it extends a subclass of
  /// `XCTestCase`).
  private func isTestClass(_ preciseIdentifier: String) -> Bool {
    if let known = testCaseClassCache[preciseIdentifier] {
      return known
    }

    guard let inheritanceRelationship = inheritanceRelationships[preciseIdentifier] else {
      // If there are no inheritance relationships with the identifier as the source, then the class
      // is either a root class or we didn't process the symbol graph for the module that declares
      // it. In either case, we can't go any further so we mark the class as not-a-test.
      testCaseClassCache[preciseIdentifier] = false
      return false
    }

    if inheritanceRelationship.target == xcTestCasePreciseIdentifier {
      // If the inheritance relationship has the precise identifier for `XCTest.XCTestCase` as its
      // target, then we know definitively that the class is a direct subclass of `XCTestCase`.
      testCaseClassCache[preciseIdentifier] = true
      return true
    }

    // If the inheritance relationship had some other class as its target (the superclass), then
    // (inductively) the source (subclass) is a test class if the superclass is.
    return isTestClass(inheritanceRelationship.target)
  }
}
