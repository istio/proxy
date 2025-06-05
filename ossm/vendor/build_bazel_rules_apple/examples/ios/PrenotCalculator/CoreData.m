// Copyright 2015 The Bazel Authors. All rights reserved.
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

#import "examples/ios/PrenotCalculator/CoreData.h"

@implementation CoreData
- (id)init {
  self = [super init];
  if (self) {

  }
  return self;
}

- (void)verify {
  NSURL *modelURL = [self modelURL];
  NSAssert(modelURL,
           @"Unable to find modelURL given mainBundle: %@",
           [[NSBundle mainBundle] description]);
}

#pragma mark Private Methods.

- (NSURL *)modelURL {
  NSBundle *bundle = [NSBundle bundleForClass:[CoreData class]];
  NSURL *modelURL = [bundle URLForResource:@"DataModel" withExtension:@"momd"];
  return modelURL;
}

@end
