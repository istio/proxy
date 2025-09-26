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

// This example prints the contents of `CommandLine.arguments` (so, the invoking
// command followed by any other arguments) one after the other in one-second
// intervals.
//
// NOTE: Do not run this target using "bazel run". Instead, build it and then
// run the resulting executable manually. Bazel adds additional output buffering
// that interferes with the timed nature of this example.

import Dispatch
#if os(Linux)
import Glibc
#elseif os(Windows)
import ucrt
#else
import Darwin
#endif

extension DispatchQueue {
  /// Enqueues a block to execute asynchronously after a deadline in a given
  /// dispatch group.
  ///
  /// This is a handy helper function that was written because the existing
  /// `DispatchQueue` APIs only support async after a deadline or async in a
  /// group, but not both at the same time.
  ///
  /// - Parameters:
  ///   - deadline: The time after which the block should execute.
  ///   - group: The dispatch group with which the block should enter and leave.
  ///   - work: The block to execute.
  func asyncAfter(
    deadline: DispatchTime,
    group: DispatchGroup,
    execute work: @escaping () -> Void
  ) {
    group.enter()
    asyncAfter(deadline: deadline) {
      work()
      group.leave()
    }
  }
}

let mainQueue = DispatchQueue.main
let group = DispatchGroup()

// Print each of the arguments in one-second intervals. Use a dispatch group to
// collect the tasks so that we can wait on their full completion below.
for (index, argument) in CommandLine.arguments.enumerated() {
  mainQueue.asyncAfter(deadline: .now() + .seconds(index), group: group) {
    print("Hello, \(argument)!")
  }
}

// Wait for all of the enqueued tasks in the group to end, then print a final
// message and exit. Note that the explicit call to `exit` is required to end
// the process because `dispatchMain` never returns.
group.notify(queue: mainQueue) {
  print("Goodbye!")
  exit(0)
}

// Park the main thread, waiting for blocks to be submitted to the main queue
// and executing them.
dispatchMain()
