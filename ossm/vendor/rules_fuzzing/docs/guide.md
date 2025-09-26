<!-- omit from toc -->
# Bazel Rules User Guide

<!-- omit from toc -->
## Contents

- [Defining fuzz tests](#defining-fuzz-tests)
  - [Defining fuzz tests](#defining-fuzz-tests-1)
  - [Building and running](#building-and-running)
  - [Specifying seed corpora](#specifying-seed-corpora)
  - [Specifying dictionaries](#specifying-dictionaries)
  - [The fuzz test launcher](#the-fuzz-test-launcher)
  - [Built-in fuzzing engines](#built-in-fuzzing-engines)
  - [Configuration flags](#configuration-flags)
- [Integrating in your project](#integrating-in-your-project)
  - [Configuring the .bazelrc file](#configuring-the-bazelrc-file)
- [Advanced topics](#advanced-topics)
  - [Defining fuzzing engines](#defining-fuzzing-engines)
- [Rule reference](#rule-reference)

## Defining fuzz tests

The rule library provides support for writing *in-process* fuzz tests, which consist of a driver function that receives a generated input string and feeds it to the API under test. To make a complete fuzz test executable, the driver is linked with a fuzzing engine, which implements the test generation logic. The rule library provides out-of-the-box support for the most popular fuzzing engines (e.g., [libFuzzer][libfuzzer-doc] and [Honggfuzz][honggfuzz-doc]), and an extension mechanism to define new fuzzing engines.

A fuzzing rule wraps a raw fuzz test executable and provides additional tools, such as the specification of a corpus and dictionary and a launcher that knows how to invoke the fuzzing engine with the appropriate set of flags.

### Defining fuzz tests

A fuzz test is specified using a [`cc_fuzz_test` rule](/docs/cc-fuzzing-rules.md#cc_fuzz_test). In the most basic form, a fuzz test requires a source file that implements the fuzz driver entry point. Let's consider a simple example that fuzzes the [RE2](https://github.com/google/re2) regular expression library:

```python
# BUILD file.

load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")

cc_fuzz_test(
    name = "re2_fuzz_test",
    srcs = ["re2_fuzz_test.cc"],
    deps = [
        "@re2",
    ],
)
```

The fuzz driver implements the special `LLVMFuzzerTestOneInput` function that receives the fuzzer-generated string and uses it to drive the API under test:

```cpp
// Implementation file.

#include <cstdint>
#include <cstddef>
#include <string>

#include "re2/re2.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    RE2 re(std::string(reinterpret_cast<const char*>(data), size), RE2::Quiet);
    return 0;
}
```

### Building and running

To build a fuzz test, you need to specify which fuzzing engine and what instrumentation to use for tracking errors during the execution of the fuzzer. Let's build the RE2 fuzz test using [libFuzzer][libfuzzer-doc] and the [Address Sanitizer (ASAN)][asan-doc] instrumentation, which catches memory errors such as buffer overflows and use-after-frees:

```sh
$ bazel build -c opt --config=asan-libfuzzer //examples:re2_fuzz_test
```

You can directly invoke this fuzz test executable if you know libFuzzer's command line interface. But in practice, you don't have to. For each fuzz test `<name>`, the rules library generates a number of additional targets that provide higher-level functionality to simplify the interaction with the fuzz test.

One such target is `<name>_run`, which provides a simple engine-agnostic interface for invoking fuzz tests. Let's run our libFuzzer example:

```sh
$ bazel run -c opt --config=asan-libfuzzer //examples:re2_fuzz_test_run
```

The fuzz test will start running locally, and write the generated tests under a temporary path under `/tmp/fuzzing`. By default, the generated tests persist across runs, in order to make it easy to stop and resume runs (possibly under different engines and configurations).

Let's interrupt the fuzz test execution (Ctrl-C), and resume it using the Honggfuzz engine:

```sh
$ bazel run -c opt --config=asan-honggfuzz //examples:re2_fuzz_test_run
```

The `<name>_run` target accepts a number of engine-agnostic flags. For example, the following command runs the fuzz test with an execution timeout and on a clean slate (removing any previously generated tests). Note the extra `--` separator between Bazel's own flags and the launcher flags:

```sh
$ bazel run -c opt --config=asan-libfuzzer //examples:re2_fuzz_test_run \
      -- --clean --timeout_secs=30
```

### Specifying seed corpora

You can use the `corpus` attribute to specify a set of files that the fuzz test can use as a seed corpus when running in continuous fuzzing mode. The following example shows how to include all the files in a directory in the seed corpus:

```python
cc_fuzz_test(
    name = "fuzz_test",
    srcs = ["fuzz_test.cc"],
    corpus = glob(["fuzz_test_corpus/**"]),
)
```

Specifying a seed corpus is a [best practice][seed-corpus] that helps the fuzzer make progress faster.

### Specifying dictionaries

Similarly, you can speed up fuzzing by specifying a dictionary using the `dicts` attribute. A dictionary is a set of string tokens that the fuzzer can use to construct and mutate test inputs. The attribute accepts a list of files with one dictionary token per line specified in the [AFL/libFuzzer format](http://llvm.org/docs/LibFuzzer.html#dictionaries):

```python
cc_fuzz_test(
    name = "fuzz_test",
    srcs = ["fuzz_test.cc"],
)
```

### The fuzz test launcher

Each fuzz test `<fuzz_test>` gets a `<fuzz_test>_run` target that can be used to launch the fuzzing executable in "continuous fuzzing" mode. The launcher provides a uniform command line interface regardless of the fuzzing engine or sanitizer used.

Currently, the launcher offers the following options:

* `--[no]clean`: If set, cleans up the output directory of the target before fuzzing (default: 'false').
* `--fuzzing_output_root`: The root directory for storing all generated artifacts during fuzzing. (default: '/tmp/fuzzing')
* `--[no]regression`: If set, the script will trigger the target as a regression test. (default: 'false')
* `--timeout_secs`: The maximum duration, in seconds, of the fuzzer run launched. (default: '0', a non-negative integer)

### Built-in fuzzing engines

* `@rules_fuzzing//fuzzing/engines:libfuzzer` provides libFuzzer support. Must be used with the `libfuzzer` engine instrumentation.

* `@rules_fuzzing//fuzzing/engines:honggfuzz` provides Honggfuzz support. Must be used with the `honggfuzz` engine instrumentation. When using WORKSPACE, requires importing its dependencies using the `honggfuzz_dependencies()` WORKSPACE function.

* `@rules_fuzzing//fuzzing/engines:replay` provides a simple engine that just executes a set of test files. It can be combined with a sanitizer and can be used for regression tests or replaying crashes.

* `@rules_fuzzing//fuzzing/engines:oss_fuzz` and `:oss_fuzz_java` provide fuzzing engines that reflect the environment configuration of an [OSS-Fuzz build][bazel-oss-fuzz]. These engines are useful in the `build.sh` script of an OSS-Fuzz project. When using WORKSPACE, this requires importing its dependencies using the `oss_fuzz_dependencies()` function.

### Configuration flags

The fuzzing rules library defines the following Bazel configuration flags that affect how a fuzz test is built:

* `--@rules_fuzzing//fuzzing:cc_engine` specifies the fuzzing engine used to build and run the fuzz test. This flag should point to a `cc_fuzzing_engine` target.

* `--@rules_fuzzing//fuzzing:cc_engine_instrumentation` specifies the engine-specific instrumentation to use. Valid values are:
   * `none`: No instrumentation.
   * `libfuzzer`: The libFuzzer-specific instrumentation. It should be used in conjunction with the `@rules_fuzzing//fuzzing/engines:libfuzzer` engine.
   * `honggfuzz`: The Honggfuzz-specific instrumentation. It should be used in conjunction with the `@rules_fuzzing//fuzzing/engines:honggfuzz` engine.
   * `oss-fuzz`: The instrumentation captured from the environment during an [OSS-Fuzz build][bazel-oss-fuzz]. It uses the `$FUZZING_CFLAGS` and `$FUZZING_CXXFLAGS` variables, if set, or falls back to `$CFLAGS` and `$CXXFLAGS` otherwise.

* `--@rules_fuzzing//fuzzing:cc_engine_sanitizer` specifies the sanitizer configuration used to detect bugs. Valid values are:
   * `none`: No sanitizer instrumentation.
   * `asan`: [Address Sanitizer (ASAN)][asan-doc].
   * `msan`: [Memory Sanitizer (MSAN)][msan-doc].
   * `msan-origin-tracking`: MSAN with [origin tracking][msan-origin-tracking] enabled (useful for debugging crash reproducers; available separately due to it being 1.5-2x slower).
   * `ubsan`: [Undefined Behavior Sanitizer (UBSAN)][ubsan-doc].

* `--@rules_fuzzing//fuzzing:cc_fuzzing_build_mode` is a bool flag that specifies whether the special [`FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION` macro][fuzzing-build-mode] is defined during the build. This is turned on by default and most users should not need to change this flag.

## Integrating in your project

### Configuring the .bazelrc file

Each fuzz test is built with a fuzzing engine and instrumentation specified as [build setting flags](#configuration-flags). For example, running a fuzz test in the libFuzzer / ASAN configuration would look like:

```sh
$ bazel test \
    --//fuzzing:cc_engine=//fuzzing/engines:libfuzzer \
    --@rules_fuzzing//fuzzing:cc_engine_instrumentation=libfuzzer \
    --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan \
    //examples:re2_fuzz_test
```

This command is clearly too verbose to be used manually, so we recommend combining these options as a `--config` setting in your project's [`.bazelrc` file][bazelrc-docs]. For example, the command above can be replaced with:

```sh
$ bazel test --config=asan-libfuzzer //examples:re2_fuzz_test
```

For convenience, we define below the most common configurations that you can pick and choose for your own `.bazelrc` file:

```
# --config=asan-libfuzzer
build:asan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:libfuzzer
build:asan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=libfuzzer
build:asan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan

# --config=msan-libfuzzer
build:msan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:libfuzzer
build:msan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=libfuzzer
build:msan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=msan

# --config=ubsan-libfuzzer
build:ubsan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:libfuzzer
build:ubsan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=libfuzzer
build:ubsan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=ubsan

# --config=asan-ubsan-libfuzzer
build:asan-ubsan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:libfuzzer
build:asan-ubsan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=libfuzzer
build:asan-ubsan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan-ubsan

# --config=asan-honggfuzz
build:asan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:honggfuzz
build:asan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine_instrumentation=honggfuzz
build:asan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan

# --config=msan-honggfuzz
build:msan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:honggfuzz
build:msan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine_instrumentation=honggfuzz
build:msan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine_sanitizer=msan

# --config=ubsan-honggfuzz
build:ubsan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:honggfuzz
build:ubsan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine_instrumentation=honggfuzz
build:ubsan-honggfuzz --@rules_fuzzing//fuzzing:cc_engine_sanitizer=ubsan

# --config=asan-replay
build:asan-replay --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:replay
build:asan-replay --@rules_fuzzing//fuzzing:cc_engine_instrumentation=none
build:asan-replay --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan

# --config=asan-ubsan-replay
build:asan-ubsan-replay --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:replay
build:asan-ubsan-replay --@rules_fuzzing//fuzzing:cc_engine_instrumentation=none
build:asan-ubsan-replay --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan-ubsan

# --config=jazzer (Jazzer without sanitizer - Java only)
build:jazzer --@rules_fuzzing//fuzzing:java_engine=@rules_fuzzing//fuzzing/engines:jazzer
build:jazzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=jazzer
build:jazzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=none

# --config=asan-jazzer
build:asan-jazzer --@rules_fuzzing//fuzzing:java_engine=@rules_fuzzing//fuzzing/engines:jazzer
build:asan-jazzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=jazzer
build:asan-jazzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan

# --config=ubsan-jazzer
build:ubsan-jazzer --@rules_fuzzing//fuzzing:java_engine=@rules_fuzzing//fuzzing/engines:jazzer
build:ubsan-jazzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=jazzer
build:ubsan-jazzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=ubsan
```

## Advanced topics

### Defining fuzzing engines

> TODO: The documentation for this feature is coming soon.

A fuzzing engine launcher script receives configuration through the following environment variables:

| Variable                   | Description |
|----------------------------|-------------|
| `FUZZER_BINARY`            | The path to the fuzz target executable. |
| `FUZZER_TIMEOUT_SECS`      | If set, a positive integer representing the timeout in seconds for the entire fuzzer run. |
| `FUZZER_IS_REGRESSION`     | Set to `1` if the fuzzer should run in regression mode (just execute the input tests), or `0` if this is a continuous fuzzer run. |
| `FUZZER_DICTIONARY_PATH`   | If set, provides a path to a fuzzing dictionary file. |
| `FUZZER_SEED_CORPUS_DIR`   | If set, provides a directory path to a seed corpus. |
| `FUZZER_OUTPUT_ROOT`       | A writable path that can be used by the fuzzer during its execution (e.g., as a workspace or for generated artifacts). See the variables below for specific categories of output. |
| `FUZZER_OUTPUT_CORPUS_DIR` | A path under `FUZZER_OUTPUT_ROOT` where the new generated tests should be stored. |
| `FUZZER_ARTIFACTS_DIR`     | A path under `FUZZER_OUTPUT_ROOT` where generated crashes and other relevant artifacts should be stored. |

## Rule reference

* [`cc_fuzz_test`](/docs/cc-fuzzing-rules.md#cc_fuzz_test)
* [`cc_fuzzing_engine`](/docs/cc-fuzzing-rules.md#cc_fuzzing_engine)

<!-- Links -->

[asan-doc]: https://clang.llvm.org/docs/AddressSanitizer.html
[bazel-oss-fuzz]: https://google.github.io/oss-fuzz/getting-started/new-project-guide/bazel/
[bazelrc-docs]: https://docs.bazel.build/versions/master/guide.html#bazelrc-the-bazel-configuration-file
[fuzzing-build-mode]: https://llvm.org/docs/LibFuzzer.html#fuzzer-friendly-build-mode
[honggfuzz-doc]: https://github.com/google/honggfuzz
[libfuzzer-doc]: https://llvm.org/docs/LibFuzzer.html
[msan-doc]: https://clang.llvm.org/docs/MemorySanitizer.html
[msan-origin-tracking]: https://clang.llvm.org/docs/MemorySanitizer.html#origin-tracking
[seed-corpus]: https://github.com/google/fuzzing/blob/master/docs/good-fuzz-target.md#seed-corpus
[ubsan-doc]: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
