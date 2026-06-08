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

/// A toy value.
@frozen
public struct ToyValue {
  /// The numeric value.
  public var number: Int

  /// The hexadecimal value of the numeric value.
  public var stringValue: String {
    "\(number)"
  }

  /// Creates a new toy value with the given numeric value.
  public init(number: Int) {
    self.number = number
  }

  /// Returns the square of the receiver's numeric value.
  public func squared() -> Int {
    number * number
  }
}
