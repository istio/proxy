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

import Foundation
import XCTest

/// An XCTest observer that generates an XML file in the format described by the
/// [JUnit test result schema](https://windyroad.com.au/dl/Open%20Source/JUnit.xsd).
public final class BazelXMLTestObserver: NSObject {
  /// The file handle to which the XML content will be written.
  private let fileHandle: FileHandle

  /// The current indentation to print before each line, as UTF-8 code units.
  private var indentation: Data

  /// The default XML-generating XCTest observer, which determines the output file based on the
  /// value of the `XML_OUTPUT_FILE` environment variable.
  ///
  /// If the `XML_OUTPUT_FILE` environment variable is not set or the file at that path could not be
  /// created and opened for writing, the value of this property will be nil.
  @MainActor
  public static let `default`: BazelXMLTestObserver? = {
    guard
      let outputPath = ProcessInfo.processInfo.environment["XML_OUTPUT_FILE"],
      FileManager.default.createFile(atPath: outputPath, contents: nil, attributes: nil),
      let fileHandle = FileHandle(forWritingAtPath: outputPath)
    else {
      return nil
    }
    return .init(fileHandle: fileHandle)
  }()

  /// Creates a new XML-generating XCTest observer that writes its content to the given file handle.
  private init(fileHandle: FileHandle) {
    self.fileHandle = fileHandle
    self.indentation = Data()
  }

  /// Writes the given string to the observer's file handle.
  private func writeLine<S: StringProtocol>(_ string: S) {
    if !indentation.isEmpty {
      fileHandle.write(indentation)
    }
    fileHandle.write(string.data(using: .utf8)!)  // Conversion to UTF-8 cannot fail.
    fileHandle.write(Data([UInt8(ascii: "\n")]))
  }

  /// Increases the current indentation level by two spaces.
  private func indent() {
    indentation.append(contentsOf: [UInt8(ascii: " "), UInt8(ascii: " ")])
  }

  /// Reduces the current indentation level by two spaces.
  private func dedent() {
    indentation.removeLast(2)
  }

  /// Canonicalizes the name of the test case for printing into the XML file.
  ///
  /// The canonical name of the test is `TestClass.testMethod` (i.e., Swift-style syntax). When
  /// running tests under the Objective-C runtime, the test cases will have Objective-C-style names
  /// (i.e., `-[TestClass testMethod]`), so this method converts those to the desired syntax.
  ///
  /// Any test name that does not match one of those two syntaxes is returned unchanged.
  private func canonicalizedName(of testCase: XCTestCase) -> String {
    let name = testCase.name
    guard name.hasPrefix("-[") && name.hasSuffix("]") else {
      return name
    }

    let trimmedName = name.dropFirst(2).dropLast()
    guard let spaceIndex = trimmedName.lastIndex(of: " ") else {
      return String(trimmedName)
    }

    return "\(trimmedName[..<spaceIndex]).\(trimmedName[trimmedName.index(after: spaceIndex)...])"
  }
}

extension BazelXMLTestObserver: XCTestObservation {
  public func testBundleWillStart(_ testBundle: Bundle) {
    writeLine(#"<?xml version="1.0" encoding="utf-8"?>"#)
    writeLine("<testsuites>")
    indent()
  }

  public func testBundleDidFinish(_ testBundle: Bundle) {
    dedent()
    writeLine("</testsuites>")
  }

  public func testSuiteWillStart(_ testSuite: XCTestSuite) {
    writeLine(
      #"<testsuite name="\#(xmlEscaping: testSuite.name)" tests="\#(testSuite.testCaseCount)">"#)
    indent()
  }

  public func testSuiteDidFinish(_ testSuite: XCTestSuite) {
    dedent()
    writeLine("</testsuite>")
  }

  public func testCaseWillStart(_ testCase: XCTestCase) {
    writeLine(
      #"<testcase name="\#(xmlEscaping: canonicalizedName(of: testCase))" status="run" "#
      + #"result="completed">"#)
    indent()
  }

  public func testCaseDidFinish(_ testCase: XCTestCase) {
    dedent()
    writeLine("</testcase>")
  }

  // On platforms with the Objective-C runtime, we use the richer `XCTIssue`-based APIs. Anywhere
  // else, we're building with the open-source version of XCTest which has only the older
  // `didFailWithDescription` API.
  #if canImport(ObjectiveC)
    public func testCase(_ testCase: XCTestCase, didRecord issue: XCTIssue) {
      let tag: String
      switch issue.type {
      case .assertionFailure, .performanceRegression, .unmatchedExpectedFailure:
        tag = "failure"
      case .system, .thrownError, .uncaughtException:
        tag = "error"
      @unknown default:
        tag = "failure"
      }

      writeLine(#"<\#(tag) message="\#(xmlEscaping: issue.compactDescription)"/>"#)
    }
  #else
    public func testCase(
      _ testCase: XCTestCase,
      didFailWithDescription description: String,
      inFile filePath: String?,
      atLine lineNumber: Int
    ) {
      let tag = description.hasPrefix(#"threw error ""#) ? "error" : "failure"
      writeLine(#"<\#(tag) message="\#(xmlEscaping: description)"/>"#)
    }
  #endif
}

// Hacks ahead! XCTest does not declare the methods that it uses to notify observers of skipped
// tests as part of the public `XCTestObservation` protocol. Instead, they are only available on
// various framework-internal protocols that XCTest checks for conformance against at runtime.
//
// On Darwin platforms, thanks to the Objective-C runtime, we can declare protocols with the same
// names in our library and implement those methods, and XCTest will call them so that we can log
// the skipped tests in our output. Note that we have to re-specify the protocol name in the `@objc`
// attribute to remove the module name for the runtime.
//
// On non-Darwin platforms, we don't have an escape hatch because XCTest is implemented in pure
// Swift and we can't play the same runtime games, so skipped tests simply get tracked as "passing"
// there.
#if canImport(ObjectiveC)
  /// Declares the observation method that is called by XCTest in Xcode 12.5 when a test case is
  /// skipped.
  @objc(_XCTestObservationInternal)
  protocol _XCTestObservationInternal {
    func testCase(
      _ testCase: XCTestCase,
      wasSkippedWithDescription description: String,
      sourceCodeContext: XCTSourceCodeContext?)
  }

  extension BazelXMLTestObserver: _XCTestObservationInternal {
    public func testCase(
      _ testCase: XCTestCase,
      wasSkippedWithDescription description: String,
      sourceCodeContext: XCTSourceCodeContext?
    ) {
      self.testCase(
        testCase,
        didRecordSkipWithDescription: description,
        sourceCodeContext: sourceCodeContext)
    }
  }

  /// Declares the observation method that is called by XCTest in Xcode 13 and later when a test
  /// case is skipped.
  @objc(_XCTestObservationPrivate)
  protocol _XCTestObservationPrivate {
    func testCase(
      _ testCase: XCTestCase,
      didRecordSkipWithDescription description: String,
      sourceCodeContext: XCTSourceCodeContext?)
  }

  extension BazelXMLTestObserver: _XCTestObservationPrivate {
    public func testCase(
      _ testCase: XCTestCase,
      didRecordSkipWithDescription description: String,
      sourceCodeContext: XCTSourceCodeContext?
    ) {
      writeLine(#"<skipped message="\#(xmlEscaping: description)"/>"#)
    }
  }
#endif
