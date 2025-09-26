#pragma once

// This component provides the release version of this library.

namespace datadog {
namespace tracing {

// The release version at or before this code revision, e.g. "v0.1.12".
// That is, this code is at least as recent as `tracer_version`, but may be
// more recent.
extern const char *const tracer_version;

// A string literal that contains `tracer_version` but also is easier to `grep`
// from the output of the `strings` command line utility, e.g. "[dd-trace-cpp
// version v0.1.12]".
extern const char *const tracer_version_string;

}  // namespace tracing
}  // namespace datadog
