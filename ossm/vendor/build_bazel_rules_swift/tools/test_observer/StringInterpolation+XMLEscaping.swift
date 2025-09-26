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

extension String.StringInterpolation {
  /// Appends the given string to a string interpolation, escaping any characters with special XML
  /// meanings.
  mutating func appendInterpolation<S: StringProtocol>(xmlEscaping string: S) {
    var remainder = string[...]
    while let escapeIndex = remainder.firstIndex(where: { xmlEscapeMapping[$0] != nil }) {
      appendLiteral(String(remainder[..<escapeIndex]))
      appendLiteral(xmlEscapeMapping[remainder[escapeIndex]]!)
      remainder = remainder[remainder.index(after: escapeIndex)...]
    }
    if !remainder.isEmpty {
      appendLiteral(String(remainder))
    }
  }
}

/// The mapping from characters with special meanings in XML to their escaped form.
private let xmlEscapeMapping: [Character: String] = [
  "\"": "&quot;",
  "'": "&apos;",
  "&": "&amp;",
  "<": "&lt;",
  ">": "&gt;",
]
