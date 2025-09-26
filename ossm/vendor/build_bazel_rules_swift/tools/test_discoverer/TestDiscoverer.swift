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

import ArgumentParser
import Foundation
import SymbolKit

@main
struct TestDiscoverer: ParsableCommand {
  /// A parsed module name and output path pair passed to the test discovery tool using the
  /// `--module-output <module-name>=<output-path>` flag.
  struct ModuleOutput: ExpressibleByArgument {
    /// The name of the module.
    var moduleName: String

    /// The file URL to a `.swift` source file that should be created or overwritten with the
    /// discovered test entries.
    var outputURL: URL

    init?(argument: String) {
      let components = argument.split(separator: "=", maxSplits: 1)
      guard components.count == 2 else { return nil }

      self.moduleName = String(components[0])
      self.outputURL = URL(fileURLWithPath: String(components[1]))
    }
  }

  @Argument(help: "Paths to directories containing symbol graph JSON files.")
  var symbolGraphDirectories: [String] = []

  @Option(help: "The path to the '.swift' file where the main test runner should be generated.")
  var mainOutput: String

  @Option(
    help: .init(
      """
      The name of a module containing tests and the path to the '.swift' file where the test entries
      for that module should be generated, in the form '<module name>=<output path>'. Must be
      specified at least once.
      """,
      valueName: "module-name-output-path-mapping"))
  var moduleOutput: [ModuleOutput] = []

  func validate() throws {
    guard !moduleOutput.isEmpty else {
      throw ValidationError("At least one '--module-output' must be provided.")
    }
    guard !symbolGraphDirectories.isEmpty else {
      throw ValidationError("At least one symbol graph directory must be provided.")
    }
  }

  mutating func run() throws {
    let collector = SymbolCollector()

    for directoryPath in symbolGraphDirectories {
      // Each symbol graph directory might contain multiple files, all of which need to be parsed;
      // there are files for symbols declared in the module itself and for symbols that represent
      // extensions to types declared in other modules.
      for url in try FileManager.default.contentsOfDirectory(
        at: URL(fileURLWithPath: directoryPath),
        includingPropertiesForKeys: nil)
      {
        let jsonData = try Data(contentsOf: url)
        let decoder = JSONDecoder()
        let symbolGraph = try decoder.decode(SymbolGraph.self, from: jsonData)
        collector.consume(symbolGraph)
      }
    }

    // For each module, print the list of test entries that were discovered in a source file that
    // extends that module.
    let testPrinter = TestPrinter(discoveredTests: collector.discoveredTests())
    for output in moduleOutput {
      testPrinter.printTestEntries(forModule: output.moduleName, toFileAt: output.outputURL)
    }

    // Print the runner source file, which implements the `@main` type that executes the tests.
    let mainFileURL = URL(fileURLWithPath: mainOutput)
    testPrinter.printTestRunner(toFileAt: mainFileURL)
  }
}
