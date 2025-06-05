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

#import "examples/ios/PrenotCalculator/AppDelegate.h"

#import "examples/ios/PrenotCalculator/CalculatorViewController.h"
#import "examples/ios/PrenotCalculator/ValuesViewController.h"

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  [self validateBundleLibrary];

  UITabBarController *bar = [[UITabBarController alloc] init];
  [bar setViewControllers:
      @[[[CalculatorViewController alloc] init], [[ValuesViewController alloc] init]]];
  bar.selectedIndex = 0;
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.rootViewController = bar;
  [self.window makeKeyAndVisible];
  return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {}

- (void)applicationDidEnterBackground:(UIApplication *)application {}

- (void)applicationWillEnterForeground:(UIApplication *)application {}

- (void)applicationDidBecomeActive:(UIApplication *)application {}

- (void)applicationWillTerminate:(UIApplication *)application {}

- (void)validateBundleLibrary {
  NSString *bundlePath = [[[NSBundle mainBundle] bundlePath]
      stringByAppendingPathComponent:@"PrenotCalculatorResources.bundle"];
  NSBundle *bundle = [NSBundle bundleWithPath:bundlePath];
  NSString *testPath = [bundle pathForResource:@"test" ofType:@"txt"];
  NSString *testContents = [NSString stringWithContentsOfFile:testPath
                                                     encoding:NSUTF8StringEncoding
                                                        error:NULL];
  NSAssert([testContents hasSuffix:@"It worked!\n"],
           @"Unable to find file given mainBundle: %@",
           [[NSBundle mainBundle] description]);
}

@end
