// Copyright 2019 The Bazel Authors. All rights reserved.
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

import ServiceClient
import ServiceTestClient
import GRPC
import NIOCore
import NIOPosix
import XCTest

class UnitTest: XCTestCase {
  func testGetWithFakeClient() throws {

    // Set up the fake contents:
    let fakeContents = "Response"
    let fakeResponse = Service_EchoResponse.with {
      $0.contents = fakeContents
    }
    let fakeClient = Service_EchoServiceTestClient()
    fakeClient.enqueueEchoResponse(fakeResponse)
    let client: Service_EchoServiceClientProtocol = fakeClient

    // Make the fake request:
    let completed = self.expectation(description: "'Get' completed")
    let call = client.echo(.with { $0.contents = fakeContents })
    call.response.whenComplete { result in
      switch result {
      case let .success(response):
        XCTAssertEqual(response.contents, fakeContents)
      case let .failure(error):
        XCTFail("Unexpected error \(error)")
      }

      completed.fulfill()
    }

    self.wait(for: [completed], timeout: 10.0)
  }
}
