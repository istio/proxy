// Copyright 2018 The Bazel Authors. All rights reserved.
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

import XCTest

class SimpleTests: XCTestCase {
  var value: Int = 0

  override func setUp() {
    value = 4
  }

  func testThatWillSucceed() {
    XCTAssertEqual(value, 4)
  }

  func testThatWillFailIfChanged() {
    // Change the second argument from 4 to something else to see this test
    // fail. We don't make it fail in the submitted code because we don't want
    // TAP to go red for an intentional failure.
    XCTAssertEqual(value, 4)
  }

  func testThatEnvVariablesArePassed() throws {
    XCTAssertEqual(ProcessInfo.processInfo.environment["XCTEST_ENV_VAR"], "TRUE")
    let bindirEnvVar: String = ProcessInfo.processInfo.environment["XCTEST_BINDIR_ENV_VAR"] ?? ""
    XCTAssertNotEqual(bindirEnvVar, "$(BINDIR)")
    let bindirURL: URL = try XCTUnwrap(URL(string: bindirEnvVar))
    XCTAssertTrue(bindirURL.pathComponents.count > 0)
  }
}
