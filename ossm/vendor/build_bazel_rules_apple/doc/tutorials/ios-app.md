# Bazel Tutorial: Build an iOS App

This tutorial covers how to build a simple iOS app using Bazel.

## What you'll learn

In this tutorial, you learn how to:

*   Set up the environment by installing Bazel and Xcode, and downloading the
    sample project
*   Set up a Bazel [workspace](https://bazel.build/concepts/build-ref#workspace) that contained the source code
    for the app and a `WORKSPACE` file that identifies the top level of the
    workspace directory
*   Update the `WORKSPACE` file to contain references to the required
    external dependencies
*   Create a `BUILD` file
*   Run Bazel to build the app for the simulator and an iOS device
*   Run the app in the simulator and on an iOS device

## Set up your environment

To get started, install Bazel and Xcode, and get the sample project.

### Install Bazel

Follow the [installation instructions](https://bazel.build/install) to install Bazel and
its dependencies.

### Install Xcode

Download and install [Xcode](https://developer.apple.com/xcode/downloads/).
Xcode contains the compilers, SDKs, and other tools required by Bazel to build
Apple applications.

## Set up a Workspace

A [workspace](https://bazel.build/concepts/build-ref#workspace) is a directory that contains the
source files for one or more software projects, as well as a `WORKSPACE` file
and `BUILD` files that contain the instructions that Bazel uses to build
the software. The workspace may also contain symbolic links to output
directories.

A workspace directory can be located anywhere on your filesystem and is denoted
by the presence of the `WORKSPACE` file at its root.

Start by creating a directory that will contain your workspace and name it `rules-apple-example`
and change directory to it:

```bash
mkdir rules-apple-example
cd rules-apple-example
```

### Create a WORKSPACE file

Every workspace must have a text file named `WORKSPACE` located in the top-level
workspace directory. This file may be empty or it may contain references
to [external dependencies](https://bazel.build/docs/external) required to build the
software.

For now, you'll create an empty `WORKSPACE` file, which simply serves to
identify the workspace directory. In later steps, you'll update the file to add
external dependency information.

Enter the following at the command line:

```bash
touch WORKSPACE
open -a Xcode WORKSPACE
```

This creates and opens the empty `WORKSPACE` file in Xcode. Feel free to use any other text editor
you're more familiar with.

### Update the WORKSPACE file

To build applications for Apple devices, Bazel needs to pull the latest
[Apple build rules](https://github.com/bazelbuild/rules_apple)
from its GitHub repository. To enable this, add the following
statements to your `WORKSPACE` file:

```starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "build_bazel_rules_apple",
    sha256 = "34c41bfb59cdaea29ac2df5a2fa79e5add609c71bb303b2ebb10985f93fa20e7",
    url = "https://github.com/bazelbuild/rules_apple/releases/download/3.1.1/rules_apple.3.1.1.tar.gz",
)

load(
    "@build_bazel_rules_apple//apple:repositories.bzl",
    "apple_rules_dependencies",
)

apple_rules_dependencies()

load(
    "@build_bazel_rules_swift//swift:repositories.bzl",
    "swift_rules_dependencies",
)

swift_rules_dependencies()

load(
    "@build_bazel_rules_swift//swift:extras.bzl",
    "swift_rules_extra_dependencies",
)

swift_rules_extra_dependencies()

load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)

apple_support_dependencies()
```

Note: Always use the
[latest version of the Apple rules](https://github.com/bazelbuild/rules_apple/releases)
in the `url` and `sha256` attributes. Make sure to check the latest dependencies required in
`rules_apple`'s [project](https://github.com/bazelbuild/rules_apple).

## Add basic Swift source files

Create a new directory named `Sources` by executing `mkdir Sources` in your terminal. This directory will contain a basic Swift source file for a simple iOS application built in SwiftUI. Execute `touch Sources/BazelApp.swift` and open the newly created file in a Text Editor to paste the following code:

```swift
import SwiftUI

@main
struct BazelApp: App {
    var body: some Scene {
        WindowGroup {
            Text("Hello from Bazel!")
        }
    }
}
```

Note: [bazel-ios-swiftui-template](https://github.com/mattrobmattrob/bazel-ios-swiftui-template) contains a template for a SwiftUI iOS application that builds with Bazel if you want to speed up this process for future usages.

## Create a BUILD file

Create and open a new `BUILD` file for editing:

```bash
touch BUILD
open -a Xcode BUILD
```

### Add the rule load statement

To build iOS targets, Bazel needs to load build rules from its GitHub repository
whenever the build runs. To make these rules available to your project, add the
following load statements to the beginning of your `BUILD` file:

```starlark
load("@build_bazel_rules_apple//apple:ios.bzl", "ios_application")
load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
```

### Add a `swift_library` rule

Bazel provides several build rules that you can use to build an app for 
Apple platforms. For this tutorial, you'll first use the
[`swift_library`](https://github.com/bazelbuild/rules_swift/blob/master/doc/rules.md#swift_library) rule to tell Bazel
how to build a Swift library. Then
you'll use the
[`ios_application`](https://github.com/bazelbuild/rules_apple/blob/master/doc/rules-ios.md#ios_application)
rule to tell it how to build the iOS application binary and the `.ipa` bundle.

Add the following to your `BUILD` file:

```starlark
swift_library(
    name = "lib",
    srcs = glob(["Sources/*.swift"]),
)
```

Note the name of the rule, `lib`. We make use of the `glob` function to include all Swift files in the `Sources` directory. This makes it so we don't need to manually add every single new file we might create in the future.

### Add an `ios_application` rule

The
[`ios_application`](https://github.com/bazelbuild/rules_apple/tree/main/doc)
rule builds the application binary and creates the `.ipa` bundle file.

First of all, create a new `Resources` directory with a `Info.plist` file which contains some metadata for the app.
It's common to organize other resources such as Asset Catalogs in the same directory.

```bash
mkdir Resources
touch Resources/Info.plist
open -a Xcode Resources/Info.plist
```

Open the newly created file and paste in it the contents found in this [example Info.plist file](https://github.com/bazelbuild/rules_apple/blob/master/examples/ios/HelloWorldSwift/Info.plist).

Add the following to your `BUILD` file created in the previous step:

```starlark
ios_application(
    name = "iOSApp",
    bundle_id = "build.bazel.rules-apple-example",
    families = [
        "iphone",
        "ipad",
    ],
    infoplists = ["Resources/Info.plist"],
    minimum_os_version = "17.0",
    visibility = ["//visibility:public"],
    deps = [":lib"],
)
```

Note: Please update the `minimum_os_version` attribute to the minimum
version of iOS that you plan to support.

Note how the `deps` attribute references the `lib` rule
you added to the `BUILD` file above.

## Build and deploy the app

You are now ready to build your app and deploy it to a Simulator and onto an
iOS device.

### Build the app for the Simulator

To build the sample app that we just created:

```bash
bazel build //:iOSApp
```

Bazel builds the sample app. During the build process, the
output will appear similar to the following:

```bash
INFO: Found 1 target...
Target //:iOSApp up-to-date:
  bazel-bin/iOSApp.ipa
INFO: Elapsed time: 1.999s, Critical Path: 1.89s
```

### Find the build outputs

The `.ipa` file and other outputs are located in the
`bazel-out/ios-sim_arm64-min17.0-applebin_ios-ios_sim_arm64-fastbuild-ST-b6790d224f6d/bin/iOSApp.ipa` directory.

### Build the app in the Simulator

`rules_apple` supports running an app directly in the iOS Simulator.
Replace `build` with `run` in the previous command to both build and
run the application:

```bash
bazel run //:iOSApp
```

Note: [`--ios_simulator_device`](https://bazel.build/reference/command-line-reference#flag--ios_simulator_device) and [`--ios_simulator_version`](https://bazel.build/reference/command-line-reference#flag--ios_simulator_version) control which
version and device will be used when launching the app.

### Generate an Xcode project

There are a few community-provided solutions (such as [rules_xcodeproj](https://github.com/buildbuddy-io/rules_xcodeproj)
) to help generating Xcode projects. By doing so, you will be able to write,
debug, and test iOS/macOS/watchOS/tvOS applications as if you were using the
Xcode build system.

Let's see how to do so with `rules_xcodeproj`.

Open the `WORKSPACE` file again and add the following:

```starlark
http_archive(
    name = "rules_xcodeproj",
    sha256 = "f5c1f4bea9f00732ef9d54d333d9819d574de7020dbd9d081074232b93c10b2c",
    url = "https://github.com/MobileNativeFoundation/rules_xcodeproj/releases/download/1.13.0/release.tar.gz",
)

load(
    "@rules_xcodeproj//xcodeproj:repositories.bzl",
    "xcodeproj_rules_dependencies",
)

xcodeproj_rules_dependencies()

load("@bazel_features//:deps.bzl", "bazel_features_deps")

bazel_features_deps()
```

Add the following import at the top of the `BUILD` file:

```starlark
load(
    "@rules_xcodeproj//xcodeproj:defs.bzl",
    "top_level_target",
    "xcodeproj",
)
```

We can now define the rule that will generate the Xcode project:

```starlark
xcodeproj(
    name = "xcodeproj",
    build_mode = "bazel",
    project_name = "iOSApp",
    tags = ["manual"],
    top_level_targets = [
        ":iOSApp",
    ],
)
```

To generate the Xcode project, invoke this rule with the following command:

```bash
bazel run //:xcodeproj
```

You should be able to open the generated `iOSApp.xcodeproj` (e.g. `xed iOSApp.xcodeproj`) and do all the usual
operations of building and testing in Xcode.

### Build the app for a device

If you want to distribute your app or install it on a physical device,
you will need to correctly set up provisioning profiles and distribution certificates.
Feel free to skip this section or come back to it at a later point.

To build your app so that it installs and launches on an iOS device, Bazel needs
the appropriate provisioning profile for that device model. Do the following:

1. Go to your [Apple Developer Account](https://developer.apple.com/account)
   and download the appropriate provisioning profile for your device. See
   [Apple's documentation](https://developer.apple.com/library/ios/documentation/IDEs/Conceptual/AppDistributionGuide/MaintainingProfiles/MaintainingProfiles.html)
   for more information.

2. Move your profile into `$WORKSPACE`.

3. (Optional) Add your profile to your `.gitignore` file.

4. Add the following line to the `ios_application` target in your `BUILD` file:

   ```starlark
   provisioning_profile = "<your_profile_name>.mobileprovision",
   ```

Note: Ensure the profile is correct so that the app can be installed on a
device.

Now build the app for your device:

```bash
bazel build //:iOSApp --ios_multi_cpus=arm64
```

This builds the app as a fat binary. To build for a specific device
architecture, designate it in the build options.

To build for a specific Xcode version, use the `--xcode_version` option. To
build for a specific SDK version, use the `--ios_sdk_version` option. The
`--xcode_version` option is sufficient in most scenarios.

To specify a minimum required iOS version, add the `minimum_os_version`
parameter to the `ios_application` build rule in your `BUILD` file.

You should also update the previously defined `xcodeproj` rule to specify
support for building for a device:

```starlark
xcodeproj(
    name = "xcodeproj",
    build_mode = "bazel",
    project_name = "iOSApp",
    tags = ["manual"],
    top_level_targets = [
        top_level_target(":iOSApp", target_environments = ["device", "simulator"]),
    ],
)
```

Note: A more advanced integration for provisioning profiles can be achieved using
the [`provisioning_profile_repository`](https://github.com/bazelbuild/rules_apple/blob/master/doc/rules-apple.md#provisioning_profile_repository)
and [`local_provisioning_profile`](https://github.com/bazelbuild/rules_apple/blob/master/doc/rules-apple.md#local_provisioning_profile)
rules.

### Install the app on a device

You can install the app on a physical device using a bazel run command when targeting a specific device architecture:

```bash
bazel run //:iOSApp --ios_multi_cpus=arm64
```

The runner will find any available physical device with an OS version higher than `minimum_os_version`.
To specify a particular device, use this flag: `--@build_bazel_rules_apple//apple/build_settings:ios_device=<uuid|ecid|serial_number|udid|name|dns_name>`.
Alternatively, add this flag alias to your `.bazelrc` file: `common --flag_alias=ios_device=@rules_apple//apple/build_settings:ios_device`. Then you can use it like this: `bazel run //:iOSApp --ios_device=<uuid|ecid|serial_number|udid|name|dns_namee>`.
To see a list of available devices, use `xcrun devicectl list devices`.

Another way to install the app on the device is to launch Xcode and use the
`Window > Devices and Simulators` command. Select your plugged-in device from the list on the
left, then add the app by clicking the **Add** (plus sign) button under
"Installed Apps" and selecting the `.ipa` file that you built.

If your app fails to install on your device, ensure that you are specifying the
correct provisioning profile in your `BUILD` file (step 4 in the previous
section).

If your app fails to launch, make sure that your device is part of your
provisioning profile. The `View Device Logs` button on the `Devices` screen in
Xcode may provide other information as to what has gone wrong.

## Further reading

For more details, see
all the [examples](https://github.com/bazelbuild/rules_apple/tree/master/examples) in this repo.
