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

// The Swift compiler generates an Objective-C header at a path of the form
// "bazel-genfiles/<target_package>/<target_name>-Swift.h". Since the Obj-C
// rules automatically add "bazel-genfiles" to the include path, the file should
// be imported using a workspace-relative path to access the Obj-C interface for
// the Swift code.
#import "examples/apple/objc_interop/generated_header/Printer-Swift.h"

int main(int argc, char **argv) {
  @autoreleasepool {
    OIPrinter *printer = [[OIPrinter alloc] initWithPrefix:@"*** "];
    [printer print:@"Hello world"];
  }
  return 0;
}
