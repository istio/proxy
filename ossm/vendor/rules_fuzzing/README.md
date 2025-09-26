# Bazel Rules for Fuzz Tests

This repository contains [Bazel](https://bazel.build/) [Starlark extensions](https://docs.bazel.build/versions/master/skylark/concepts.html) for defining fuzz tests in Bazel projects.

[Fuzzing](https://en.wikipedia.org/wiki/Fuzzing) is an effective technique for uncovering security and stability bugs in software. Fuzzing works by invoking the code under test (e.g., a library API) with automatically generated data, and observing its execution to discover incorrect behavior, such as memory corruption or failed invariants. Read more [here](https://github.com/google/fuzzing) about fuzzing, additional examples, best practices, and other resources.

The rule library currently provides support for C++ and Java fuzz tests. Support for additional languages may be added in the future.

## Features at a glance

* C++ and Java fuzzing, with several fuzzing engines supported out of the box:
  * C++: [libFuzzer][libfuzzer-doc] and [Honggfuzz][honggfuzz-doc]
  * Java: [Jazzer][jazzer-doc]
* Multiple sanitizer configurations:
  * [Address Sanitizer][asan-doc]
  * [Memory Sanitizer][msan-doc]
  * [Undefined Behavior Sanitizer][ubsan-doc]
* Corpora and dictionaries.
* Simple "bazel run/test" commands to build and run the fuzz tests.
  * No need to understand the details of each fuzzing engine.
  * No need to explicitly manage its corpus or dictionary.
* Out-of-the-box [OSS-Fuzz](https://github.com/google/oss-fuzz) support that [substantially simplifies][bazel-oss-fuzz] the project integration effort.
* Regression testing support, useful in continuous integration.
* Customization options:
  * Defining additional fuzzing engines.
  * Customizing the behavior of the fuzz test rule.

Contributions are welcome! Please read the [contribution guidelines](/docs/contributing.md).

## Getting started

This section will walk you through the steps to set up fuzzing in your Bazel project and write your first fuzz test. We assume Bazel [is installed](https://docs.bazel.build/versions/main/install.html) on your machine.

### Prerequisites

The fuzzing rules have been tested on Bazel 4.0.0 or later. Check your Bazel version by running `bazel --version`.

C++ fuzzing requires a Clang compiler. The libFuzzer engine requires at least Clang 6.0. In addition, the Honggfuzz engine requires the `libunwind-dev` and `libblocksruntime-dev` packages:

```sh
$ sudo apt-get install clang libunwind-dev libblocksruntime-dev
```

Java fuzzing requires Clang and the LLD linker:

```sh
$ sudo apt-get install clang lld
```

### Configuring the WORKSPACE

Add the following to your `WORKSPACE` file:

```python
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_fuzzing",
    sha256 = "23bb074064c6f488d12044934ab1b0631e8e6898d5cf2f6bde087adb01111573",
    strip_prefix = "rules_fuzzing-0.3.1",
    urls = ["https://github.com/bazelbuild/rules_fuzzing/archive/v0.3.1.zip"],
)

load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")

rules_fuzzing_dependencies()

load("@rules_fuzzing//fuzzing:init.bzl", "rules_fuzzing_init")

rules_fuzzing_init()

load("@fuzzing_py_deps//:requirements.bzl", "install_deps")

install_deps()
```

> NOTE: Replace this snippet with the [latest release instructions](https://github.com/bazelbuild/rules_fuzzing/releases/latest). To get the latest unreleased features, you may need to change the `urls` and `sha256` attributes to fetch from `HEAD`. For more complex `WORKSPACE` files, you may also need to reconcile conflicting dependencies; read more in the [Bazel documentation](https://docs.bazel.build/versions/master/external.html).


### Configuring the .bazelrc file

It is best to create command shorthands for the fuzzing configurations you will use during development. In our case, let's create a configuration for libFuzzer + Address Sanitizer. In your `.bazelrc` file, add the following:

```
# Force the use of Clang for C++ builds.
build --action_env=CC=clang
build --action_env=CXX=clang++

# Define the --config=asan-libfuzzer configuration.
build:asan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine=@rules_fuzzing//fuzzing/engines:libfuzzer
build:asan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=libfuzzer
build:asan-libfuzzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan
```

Examples for other combinations of fuzzing engine and sanitizer can be found in the [User Guide](/docs/guide.md#configuring-the-bazelrc-file).

### Defining a C++ fuzz test

A C++ fuzz test is specified using a [`cc_fuzz_test` rule](/docs/cc-fuzzing-rules.md#cc_fuzz_test). In the most basic form, a fuzz test requires a source file that implements the fuzz driver entry point.

Let's create a fuzz test that exhibits a buffer overflow. Create a `fuzz_test.cc` file in your workspace root, as follows:

```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>

void TriggerBufferOverflow(const uint8_t *data, size_t size) {
  if (size >= 3 && data[0] == 'F' && data[1] == 'U' && data[2] == 'Z' &&
      data[size] == 'Z') {
    fprintf(stderr, "BUFFER OVERFLOW!\n");
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  TriggerBufferOverflow(data, size);
  return 0;
}
```

Let's now define its build target in the `BUILD` file:

```python
load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")

cc_fuzz_test(
    name = "fuzz_test",
    srcs = ["fuzz_test.cc"],
)
```

### Running the fuzz test

You can now build and run the fuzz test. For each fuzz test `<name>` defined, the framework automatically generates a launcher tool `<name>_run` that will build and run the fuzz test according to the configuration specified:

```sh
$ bazel run --config=asan-libfuzzer //:fuzz_test_run
```

Our libFuzzer test will start running and immediately discover the buffer overflow issue in the code:

```
INFO: Seed: 2957541205
INFO: Loaded 1 modules   (8 inline 8-bit counters): 8 [0x5aab10, 0x5aab18),
INFO: Loaded 1 PC tables (8 PCs): 8 [0x5aab18,0x5aab98),
INFO:      755 files found in /tmp/fuzzing/corpus
INFO:        0 files found in fuzz_test_corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 35982 bytes
INFO: seed corpus: files: 755 min: 1b max: 35982b total: 252654b rss: 35Mb
#756    INITED cov: 6 ft: 7 corp: 4/10b exec/s: 0 rss: 47Mb
=================================================================
==724294==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000047a74 at pc 0x0000005512d9 bp 0x7fff3049d270 sp 0x7fff3049d268
```

The crash is saved under `/tmp/fuzzing/artifacts` and can be further inspected.

### Java fuzzing

You can write `java_fuzz_test`s through the [Jazzer][jazzer-doc] fuzzing engine.

To use Jazzer, it is convenient to also define a `.bazelrc` configuration, similar to the C++ libFuzzer one above:

```
# Force the use of Clang for all builds (Jazzer requires at least Clang 9).
build --action_env=CC=clang
build --action_env=CXX=clang++

# Define --config=jazzer for Jazzer without sanitizer (Java only).
build:jazzer --@rules_fuzzing//fuzzing:java_engine=@rules_fuzzing//fuzzing/engines:jazzer
build:jazzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=jazzer
build:jazzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=none

# Define --config=asan-jazzer for Jazzer + ASAN.
build:asan-jazzer --@rules_fuzzing//fuzzing:java_engine=@rules_fuzzing//fuzzing/engines:jazzer
build:asan-jazzer --@rules_fuzzing//fuzzing:cc_engine_instrumentation=jazzer
build:asan-jazzer --@rules_fuzzing//fuzzing:cc_engine_sanitizer=asan
```

A Java fuzz test is specified using a [`java_fuzz_test` rule](/docs/java-fuzzing-rules.md#java_fuzz_test). In the most basic form, a Java fuzz test consists of a single `.java` file with a class that defines a function `public static fuzzerTestOneInput(byte[] input)`.

Create the `src/com/example/JavaFuzzTest.java` file in your workspace root, as follows:

```java
package com.example;

public class JavaFuzzTest {
    public static void fuzzerTestOneInput(byte[] data) {
        if (data.length >= 3 && data[0] == 'F' && data[1] == 'U' &&
            data[2] == 'Z' && data[data.length] == 'Z') {
            throw new IllegalStateException(
                "ArrayIndexOutOfBoundException thrown above");
        }
    }
}
```

You should now define the corresponding target in the `BUILD` file, which looks very much like a regular `java_binary`:

```python
load("@rules_fuzzing//fuzzing:java_defs.bzl", "java_fuzz_test")

java_fuzz_test(
    name = "JavaFuzzTest",
    srcs = ["src/com/example/JavaFuzzTest.java"],
    # target_class is not needed if using the Maven directory layout.
    # target_class = "com.example.JavaFuzzTest",
)
```

You can now start the fuzzer using the Jazzer engine by running:

```sh
$ bazel run --config=jazzer //:JavaFuzzTest_run
```

Jazzer will quickly hit an `ArrayIndexOutOfBoundsException`:

```
INFO: Instrumented com.example.JavaFuzzTest (took 98 ms, size +96%)
INFO: Seed: 4010526312
INFO: Loaded 1 modules   (512 inline 8-bit counters): 512 [0x7fae23acd800, 0x7fae23acda00),
INFO: Loaded 1 PC tables (512 PCs): 512 [0x7fae226c9800,0x7fae226cb800),
INFO:       16 files found in /tmp/fuzzing/corpus
INFO:        0 files found in test/JavaFuzzTest_corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: seed corpus: files: 16 min: 1b max: 19b total: 210b rss: 199Mb
#18     INITED cov: 3 ft: 3 corp: 2/5b exec/s: 0 rss: 200Mb
...
#6665   REDUCE cov: 5 ft: 5 corp: 4/10b lim: 63 exec/s: 0 rss: 202Mb L: 3/3 MS: 3 ChangeBit-ChangeBit-EraseBytes-

== Java Exception: java.lang.ArrayIndexOutOfBoundsException: Index 3 out of bounds for length 3
    at com.example.JavaFuzzTest.fuzzerTestOneInput(JavaFuzzTest.java:5)
```

### OSS-Fuzz integration

Once you wrote and tested the fuzz test, you should run it on continuous fuzzing infrastructure so it starts generating tests and finding new crashes in your code.

The C++ and Java fuzzing rules provide out-of-the-box support for [OSS-Fuzz](https://github.com/google/oss-fuzz), free continuous fuzzing infrastructure from Google for open source projects. Read its [Bazel project guide][bazel-oss-fuzz] for detailed instructions.

## Where to go from here?

Congratulations, you have built and run your first fuzz test with the Bazel rules!

Check out the [`examples/`](examples/) directory, which showcases additional features. Read the [User Guide](/docs/guide.md) for detailed usage instructions.

<!-- Links -->

[asan-doc]: https://clang.llvm.org/docs/AddressSanitizer.html
[bazel-oss-fuzz]: https://google.github.io/oss-fuzz/getting-started/new-project-guide/bazel/
[honggfuzz-doc]: https://github.com/google/honggfuzz
[libfuzzer-doc]: https://llvm.org/docs/LibFuzzer.html
[jazzer-doc]: https://github.com/CodeIntelligenceTesting/jazzer
[msan-doc]: https://clang.llvm.org/docs/MemorySanitizer.html
[ubsan-doc]: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
