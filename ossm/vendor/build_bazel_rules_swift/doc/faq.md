# Frequently Asked Questions

## How can I build a `swift_library` directly for iOS?

By default, `swift_library` will compile code for your host machine (e.g. macOS).
As a result, you may see compiler errors if you attempt to build a `swift_library`
which imports iOS system frameworks, such as UIKit or SwiftUI:

```
error: no such module 'UIKit'
```

In order to build a `swift_library` for iOS, you have two main options:

1. (Preferred) Build the `swift_library` transitively via an `ios_*` target.

The `ios_*` rules (e.g. `ios_application`, `ios_framework`, `ios_unit_test`, `ios_build_test`, etc.) 
from **rules_apple** apply a "transition" to their transitive dependencies which
applies a number of configurations that enable them to build for the target platform.

The benefit to using these rules is that you can share a analysis and build cache with your main application,
making building targets with a lot of dependencies much faster.

2. Build the `swift_library` directly with a config.

You can alternately create a config to build for the desired platform in your `.bazelrc` file:

```
build:ios_simulator --platforms=@build_bazel_apple_support//platforms:ios_sim_arm64
```

And apply it when building:

```
bazel build //path/to/your/library --config=ios_simulator
```

The drawback to this approach is that the configs will likely differ between your application
and swift library, resulting in the caches not being shared.
