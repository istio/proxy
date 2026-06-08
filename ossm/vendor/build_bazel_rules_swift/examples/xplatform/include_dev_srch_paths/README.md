# Demonstrate `swift_library_group` and `always_include_developer_search_paths`

This example demonstrates the use of the `swift_library_group` rule and the
`always_include_developer_search_paths` attribute for `swift_library`.

## Scenario

The scenario is a developer provides a suite of utility modules that they would like to provide as a
single Bazel dependency. One module, `TestHelpers`, provides a custom assertion syntax for `XCTest`
tests. The other module, `StringHelpers`, provides functions for generating string values. They
should be made available to clients using a single target, `Helpers`.

## Combine Swift modules using `swift_library`

To provide multiple Swift modules as a single target (i.e., this is anlagous to Swift Package
Manager products), one can combine the Swift module Bazel targets using the `swift_library_group`
rule. The Swift modules provided as `deps` to the `swift_library_group` are forwarded to any target
that depends on it.

## Include developer search paths using `always_include_developer_search_paths`

The `TestHelpers` module can only provide the custom assertions if `XCTest` is available. The
`XCTest` module is special in that it is only available if the developer search paths are visible
during compilation. Historically, this would require that the `swift_library` be marked
`testonly = True`. However, because the author wants to provide their utilities as a single
dependency, marking `TestHelpers` as `testonly` would mandate that the `Helpers` target be marked as
`testonly`. This would prevent non-test targets from using the `StringHelpers` module via the
`Helpers` target.

The `always_include_developer_search_paths` attribute on the `swift_library` rule was introduced to
allow the author of the library to dictate whether the developer search paths are added during
compilation. By default, the attribute is `False`. An author must explicitly set
`always_include_developer_search_paths = True` or `testonly = True` for the developer search paths
to be availble during compilation.

### When should I use `testonly` vs `always_include_developer_search_paths`?

In short, prefer marking your target as `testonly`, if you import `XCTest`. The
`always_include_developer_search_paths` attribute was added to support Swift packages
that provide test and non-test dependencies in a single Swift product or Swift target. Marking the
corresponding Bazel targets `testonly` makes them unusable in non-test scenarios.

### Should I combine test and non-test Swift modules?

Typically, no. Prefer keeping test-specific code separate from non-test code. As The Offspring told
us ["You gotta keep 'em separated"](https://youtu.be/1jOk8dk-qaU?si=G73P7X7hf6HDluVG).
