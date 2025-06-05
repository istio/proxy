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
import ServiceServer
import GRPC
import NIOCore
import NIOPosix
import XCTest

public class EchoServiceProvider: Service_EchoServiceProvider {
  public let interceptors: Service_EchoServiceServerInterceptorFactoryProtocol?

  public init(interceptors: Service_EchoServiceServerInterceptorFactoryProtocol? = nil) {
    self.interceptors = interceptors
  }

  public func echo(
    request: ServiceServer.Service_EchoRequest,
    context: StatusOnlyCallContext)
    -> EventLoopFuture<ServiceServer.Service_EchoResponse>
  {
    let response = ServiceServer.Service_EchoResponse.with {
      $0.contents = request.contents
    }
    return context.eventLoop.makeSucceededFuture(response)
  }
}

class UnitTest: XCTestCase {

  private var group: MultiThreadedEventLoopGroup?
  private var server: Server?
  private var channel: ClientConnection?

  private func setUpServerAndChannel() throws -> ClientConnection {
    let group = MultiThreadedEventLoopGroup(numberOfThreads: 1)
    self.group = group

    let server = try Server.insecure(group: group)
      .withServiceProviders([EchoServiceProvider()])
      .bind(host: "127.0.0.1", port: 0)
      .wait()

    self.server = server

    let channel = ClientConnection.insecure(group: group)
      .connect(host: "127.0.0.1", port: server.channel.localAddress!.port!)

    self.channel = channel

    return channel
  }

  override func tearDown() {
    if let channel = self.channel {
      XCTAssertNoThrow(try channel.close().wait())
    }
    if let server = self.server {
      XCTAssertNoThrow(try server.close().wait())
    }
    if let group = self.group {
      XCTAssertNoThrow(try group.syncShutdownGracefully())
    }

    super.tearDown()
  }

  func testGetWithRealClientAndServer() throws {
    let channel = try self.setUpServerAndChannel()
    let client = Service_EchoServiceNIOClient(channel: channel)

    let completed = self.expectation(description: "'Get' completed")

    let call = client.echo(.with { $0.contents = "Hello" })
    call.response.whenComplete { result in
      switch result {
      case let .success(response):
        XCTAssertEqual(response.contents, "Hello")
      case let .failure(error):
        XCTFail("Unexpected error \(error)")
      }

      completed.fulfill()
    }

    self.wait(for: [completed], timeout: 10.0)
  }
}
