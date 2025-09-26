# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""The implementation of the {cc, java}_fuzz_test rules."""

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_fuzzing_oss_fuzz//:instrum.bzl", "native_library_sanitizer")
load("@rules_java//java:java_binary.bzl", "java_binary")
load("//fuzzing/private:binary.bzl", "fuzzing_binary", "fuzzing_binary_uninstrumented")
load("//fuzzing/private:common.bzl", "fuzzing_corpus", "fuzzing_dictionary", "fuzzing_launcher")
load("//fuzzing/private:java_utils.bzl", "determine_primary_class", "jazzer_fuzz_binary")
load("//fuzzing/private:regression.bzl", "fuzzing_regression_test")
load("//fuzzing/private/oss_fuzz:package.bzl", "oss_fuzz_package")

def fuzzing_decoration(
        name,
        raw_binary,
        engine,
        corpus = None,
        dicts = None,
        instrument_binary = True,
        define_regression_test = True,
        tags = None,
        target_compatible_with = None,
        test_size = None,
        test_tags = None,
        test_timeout = None):
    """Generates the standard targets associated to a fuzz test.

    This macro can be used to define custom fuzz test rules in case the default
    `cc_fuzz_test` macro is not adequate. Refer to the `cc_fuzz_test` macro
    documentation for the set of targets generated.

    Args:
        name: The name prefix of the generated targets. It is normally the
          fuzz test name in the BUILD file.
        raw_binary: The label of the cc_binary or cc_test of fuzz test
          executable.
        engine: The label of the fuzzing engine used to build the binary.
        corpus: A list of corpus files.
        dicts: A list of fuzzing dictionary files.
        instrument_binary: **(Experimental, may be removed in the future.)**

          By default, the generated targets depend on `raw_binary` through
          a Bazel configuration using flags from the `@rules_fuzzing//fuzzing`
          package to determine the fuzzing build mode, engine, and sanitizer
          instrumentation.

          When this argument is false, the targets assume that `raw_binary` is
          already built in the proper configuration and will not apply the
          transition.

          Most users should not need to change this argument. If you think the
          default instrumentation mode does not work for your use case, please
          file a Github issue to discuss.
        define_regression_test: If true, generate a regression test rule.
        tags: Additional tags set on non-test targets.
        target_compatible_with: Platform constraints set on the generated targets.
        test_size: The size of the fuzzing regression test.
        test_tags: Tags set on the fuzzing regression test.
        test_timeout: The timeout for the fuzzing regression test.
    """

    # We tag all non-test targets as "manual" in order to optimize the build
    # size output of test runs in RBE mode. Otherwise, "bazel test" commands
    # build all the non-test targets by default and, in remote builds, all these
    # targets and their runfiles would be transferred from the remote cache to
    # the local machine, ballooning the size of the output.
    tags = (tags or []) + ["manual"]

    instrum_binary_name = name + "_bin"
    launcher_name = name + "_run"
    corpus_name = name + "_corpus"
    dict_name = name + "_dict"

    if instrument_binary:
        fuzzing_binary(
            name = instrum_binary_name,
            binary = raw_binary,
            engine = engine,
            corpus = corpus_name,
            dictionary = dict_name if dicts else None,
            testonly = True,
            tags = tags,
            target_compatible_with = target_compatible_with,
        )
    else:
        fuzzing_binary_uninstrumented(
            name = instrum_binary_name,
            binary = raw_binary,
            engine = engine,
            corpus = corpus_name,
            dictionary = dict_name if dicts else None,
            testonly = True,
            tags = tags,
            target_compatible_with = target_compatible_with,
        )

    fuzzing_corpus(
        name = corpus_name,
        srcs = corpus,
        testonly = True,
        tags = tags,
        target_compatible_with = target_compatible_with,
    )

    if dicts:
        fuzzing_dictionary(
            name = dict_name,
            dicts = dicts,
            output = name + ".dict",
            testonly = True,
            tags = tags,
            target_compatible_with = target_compatible_with,
        )

    fuzzing_launcher(
        name = launcher_name,
        binary = instrum_binary_name,
        testonly = True,
        tags = tags,
        target_compatible_with = target_compatible_with,
    )

    if define_regression_test:
        fuzzing_regression_test(
            name = name,
            binary = instrum_binary_name,
            size = test_size,
            tags = test_tags,
            timeout = test_timeout,
            target_compatible_with = target_compatible_with,
        )

    oss_fuzz_package(
        name = name + "_oss_fuzz",
        base_name = name,
        binary = instrum_binary_name,
        testonly = True,
        tags = tags,
        target_compatible_with = target_compatible_with,
    )

def cc_fuzz_test(
        name,
        corpus = None,
        dicts = None,
        engine = Label("//fuzzing:cc_engine"),
        size = None,
        tags = None,
        target_compatible_with = None,
        timeout = None,
        **binary_kwargs):
    """Defines a C++ fuzz test and a few associated tools and metadata.

    For each fuzz test `<name>`, this macro defines a number of targets. The
    most relevant ones are:

    * `<name>`: A test that executes the fuzzer binary against the seed corpus
      (or on an empty input if no corpus is specified).
    * `<name>_bin`: The instrumented fuzz test executable. Use this target
      for debugging or for accessing the complete command line interface of the
      fuzzing engine. Most developers should only need to use this target
      rarely.
    * `<name>_run`: An executable target used to launch the fuzz test using a
      simpler, engine-agnostic command line interface.
    * `<name>_oss_fuzz`: Generates a `<name>_oss_fuzz.tar` archive containing
      the fuzz target executable and its associated resources (corpus,
      dictionary, etc.) in a format suitable for unpacking in the $OUT/
      directory of an OSS-Fuzz build. This target can be used inside the
      `build.sh` script of an OSS-Fuzz project.

    Args:
        name: A unique name for this target. Required.
        corpus: A list containing corpus files.
        dicts: A list containing dictionaries.
        engine: A label pointing to the fuzzing engine to use.
        size: The size of the regression test. This does *not* affect fuzzing
          itself. Takes the [common size values](https://bazel.build/reference/be/common-definitions#test.size).
        tags: Tags set on the generated targets.
        target_compatible_with: Platform constraints set on the generated targets.
        timeout: The timeout for the regression test. This does *not* affect
          fuzzing itself. Takes the [common timeout values](https://docs.bazel.build/versions/main/be/common-definitions.html#test.timeout).
        **binary_kwargs: Keyword arguments directly forwarded to the fuzz test
          binary rule.
    """

    # Append the '_' suffix to the raw target to dissuade users from referencing
    # this target directly. Instead, the binary should be built through the
    # instrumented configuration.
    raw_binary_name = name + "_raw_"
    binary_kwargs.setdefault("deps", [])

    # Use += rather than append to allow users to pass in select() expressions for
    # deps, which only support concatenation with +.
    # Workaround for https://github.com/bazelbuild/bazel/issues/14157.
    # buildifier: disable=list-append
    binary_kwargs["deps"] += [engine]

    cc_binary(
        name = raw_binary_name,
        tags = (tags or []) + ["manual"],
        target_compatible_with = target_compatible_with,
        **binary_kwargs
    )

    fuzzing_decoration(
        name = name,
        raw_binary = raw_binary_name,
        engine = engine,
        corpus = corpus,
        dicts = dicts,
        tags = tags,
        target_compatible_with = target_compatible_with,
        test_size = size,
        test_tags = (tags or []) + [
            "fuzz-test",
        ],
        test_timeout = timeout,
    )

_ASAN_RUNTIME = Label("//fuzzing/private/runtime:asan")
_UBSAN_RUNTIME = Label("//fuzzing/private/runtime:ubsan")
_RUNTIME_BY_NAME = {
    "asan": _ASAN_RUNTIME,
    "ubsan": _UBSAN_RUNTIME,
    "none": None,
}

# buildifier: disable=list-append
def java_fuzz_test(
        name,
        srcs = None,
        target_class = None,
        corpus = None,
        dicts = None,
        engine = Label("//fuzzing:java_engine"),
        size = None,
        tags = None,
        target_compatible_with = None,
        timeout = None,
        **binary_kwargs):
    """Defines a Java fuzz test and a few associated tools and metadata.

    For each fuzz test `<name>`, this macro defines a number of targets. The
    most relevant ones are:

    * `<name>`: A test that executes the fuzzer binary against the seed corpus
      (or on an empty input if no corpus is specified).
    * `<name>_bin`: The instrumented fuzz test executable. Use this target
      for debugging or for accessing the complete command line interface of the
      fuzzing engine. Most developers should only need to use this target
      rarely.
    * `<name>_run`: An executable target used to launch the fuzz test using a
      simpler, engine-agnostic command line interface.
    * `<name>_oss_fuzz`: Generates a `<name>_oss_fuzz.tar` archive containing
      the fuzz target executable and its associated resources (corpus,
      dictionary, etc.) in a format suitable for unpacking in the $OUT/
      directory of an OSS-Fuzz build. This target can be used inside the
      `build.sh` script of an OSS-Fuzz project.

    Args:
        name: A unique name for this target. Required.
        srcs: A list of source files of the target.
        target_class: The class that contains the static fuzzerTestOneInput
          method. Defaults to the same class main_class would.
        corpus: A list containing corpus files.
        dicts: A list containing dictionaries.
        engine: A label pointing to the fuzzing engine to use.
        size: The size of the regression test. This does *not* affect fuzzing
          itself. Takes the [common size values](https://bazel.build/reference/be/common-definitions#test.size).
        tags: Tags set on the generated targets.
        target_compatible_with: Platform constraints set on the generated targets.
        timeout: The timeout for the regression test. This does *not* affect
          fuzzing itself. Takes the [common timeout values](https://docs.bazel.build/versions/main/be/common-definitions.html#test.timeout).
        **binary_kwargs: Keyword arguments directly forwarded to the fuzz test
          binary rule.
    """

    # Append the '_' suffix to the raw target to dissuade users from referencing
    # this target directly. Instead, the binary should be built through the
    # instrumented configuration.
    raw_target_name = name + "_target_"
    metadata_binary_name = name + "_metadata_"
    metadata_deploy_jar_name = metadata_binary_name + "_deploy.jar"

    # Determine a value for target_class heuristically using the same rules as
    # those used by Bazel internally for main_class.
    # FIXME: This operates on the raw unresolved srcs list entries and thus
    #  cannot handle labels.
    if not target_class:
        target_class = determine_primary_class(srcs, name)
    if not target_class:
        fail(("Unable to determine fuzz target class for java_fuzz_test {name}" +
              ", specify target_class.").format(
            name = name,
        ))
    target_class_manifest_line = "Jazzer-Fuzz-Target-Class: %s" % target_class
    java_binary(
        name = metadata_binary_name,
        create_executable = False,
        deploy_manifest_lines = [target_class_manifest_line],
        tags = (tags or []) + ["manual"],
        target_compatible_with = target_compatible_with,
    )

    # use += rather than append to allow users to pass in select() expressions for
    # deps, which only support concatenation with +.
    # workaround for https://github.com/bazelbuild/bazel/issues/14157.
    if srcs:
        binary_kwargs.setdefault("deps", [])
        binary_kwargs["deps"] += [engine, metadata_deploy_jar_name]
    else:
        binary_kwargs.setdefault("runtime_deps", [])
        binary_kwargs["runtime_deps"] += [engine, metadata_deploy_jar_name]

    binary_kwargs.setdefault("jvm_flags", [])
    binary_kwargs["jvm_flags"] = [
        # Ensures that full stack traces are emitted for findings even in highly
        # optimized code.
        "-XX:-OmitStackTraceInFastThrow",
        # Optimized for throughput rather than latency.
        "-XX:+UseParallelGC",
        # Ignore CriticalJNINatives if not available (JDK 18+).
        "-XX:+IgnoreUnrecognizedVMOptions",
        # Improves performance of Jazzer's native compare instrumentation.
        "-XX:+CriticalJNINatives",
    ] + binary_kwargs["jvm_flags"]

    java_binary(
        name = raw_target_name,
        srcs = srcs,
        main_class = "com.code_intelligence.jazzer.Jazzer",
        tags = (tags or []) + ["manual"],
        target_compatible_with = target_compatible_with,
        **binary_kwargs
    )

    raw_binary_name = name + "_raw_"
    jazzer_fuzz_binary(
        name = raw_binary_name,
        sanitizer = select({
            Label("//fuzzing/private:is_oss_fuzz"): native_library_sanitizer,
            Label("//fuzzing/private:use_asan"): "asan",
            Label("//fuzzing/private:use_ubsan"): "ubsan",
            "//conditions:default": "none",
        }),
        sanitizer_options = select({
            Label("//fuzzing/private:is_oss_fuzz"): Label("//fuzzing/private:oss_fuzz_jazzer_sanitizer_options.sh"),
            "//conditions:default": Label("//fuzzing/private:local_jazzer_sanitizer_options.sh"),
        }),
        sanitizer_runtime = select({
            Label("//fuzzing/private:is_oss_fuzz"): _RUNTIME_BY_NAME[native_library_sanitizer],
            Label("//fuzzing/private:use_asan"): _ASAN_RUNTIME,
            Label("//fuzzing/private:use_ubsan"): _UBSAN_RUNTIME,
            "//conditions:default": None,
        }),
        target = raw_target_name,
        tags = (tags or []) + ["manual"],
        target_compatible_with = target_compatible_with,
    )

    fuzzing_decoration(
        name = name,
        raw_binary = raw_binary_name,
        engine = engine,
        corpus = corpus,
        dicts = dicts,
        tags = tags,
        target_compatible_with = target_compatible_with,
        test_size = size,
        test_tags = (tags or []) + [
            "fuzz-test",
        ],
        test_timeout = timeout,
    )
