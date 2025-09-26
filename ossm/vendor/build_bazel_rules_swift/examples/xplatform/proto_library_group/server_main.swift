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

import Dispatch
import GRPC
import NIOCore
import NIOPosix
import examples_xplatform_proto_library_group_request_request_proto
import examples_xplatform_proto_library_group_response_response_proto
import ServiceServer

/// Concrete implementation of the `EchoService` service definition.
class EchoProvider: Service_EchoServiceProvider {
  var interceptors: Service_EchoServiceServerInterceptorFactoryProtocol?

  /// Called when the server receives a request for the `EchoService.Echo` method.
  ///
  /// - Parameters:
  ///   - request: The message containing the request parameters.
  ///   - context: Information about the current session.
  /// - Returns: The response that will be sent back to the client.
  func echo(request: Request_Request,
            context: StatusOnlyCallContext) -> EventLoopFuture<Response_Response> {
    return context.eventLoop.makeSucceededFuture(Response_Response.with {
      $0.request = request
    })
  }
}

@main
struct ServerMain {
  static func main() throws {
    let group = MultiThreadedEventLoopGroup(numberOfThreads: 1)
    defer {
      try! group.syncShutdownGracefully()
    }

    // Initialize and start the service.
    let server = Server.insecure(group: group)
      .withServiceProviders([EchoProvider()])
      .bind(host: "0.0.0.0", port: 9000)

    server.map {
      $0.channel.localAddress
    }.whenSuccess { address in
      print("server started on port \(address!.port!)")
    }

    // Wait on the server's `onClose` future to stop the program from exiting.
    _ = try server.flatMap {
      $0.onClose
    }.wait()
  }
}
