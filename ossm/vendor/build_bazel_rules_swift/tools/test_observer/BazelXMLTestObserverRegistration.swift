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

#if canImport(ObjectiveC)
  import Foundation
  import XCTest

  /// The principal class in an XCTest bundle on Darwin-based platforms, which registers the
  /// XML-generating observer with the XCTest observation center when the bundle is loaded.
  @objc(BazelXMLTestObserverRegistration)
  @MainActor
  public final class BazelXMLTestObserverRegistration: NSObject {
    @objc public override init() {
      super.init()

      if let observer = BazelXMLTestObserver.default {
        XCTestObservationCenter.shared.addTestObserver(observer)
      }
    }
  }
#endif
