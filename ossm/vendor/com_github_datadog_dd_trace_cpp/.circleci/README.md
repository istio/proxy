Circle CI
=========
This directory contains the [configuration](config.yml) for this project's
[continuous integration setup][4].

The following jobs are defined:
- `format` checks that the C++ source is formatted per `clang-format-14
  --style=file`.
- `shellcheck` checks that [shellcheck][5] accepts all of the scripts in
  [bin/](../bin).
- `build-bazel` builds the library using [bazel][1].
  - Based on the `toolchain` parameter, the build will use either g++ or
    clang++.
- `test-cmake` builds the library using [CMake][2] and runs the unit tests.
  - Based on the `toolchain` parameter, the build will use either g++ or
    clang++.
  - Based on the `sanitize` parameter, the build might use [AddressSanitizer and
    UndefinedBehaviorSanitizer][3].

[1]: https://bazel.build/
[2]: https://cmake.org/
[3]: https://github.com/google/sanitizers
[4]: https://app.circleci.com/pipelines/github/DataDog/dd-trace-cpp
[5]: https://www.shellcheck.net/
