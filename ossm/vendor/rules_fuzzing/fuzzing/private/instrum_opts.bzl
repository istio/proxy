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

"""A representation for instrumentation options, along with operations."""

def _is_string_list(value):
    if type(value) != type([]):
        return False
    if any([type(element) != type("") for element in value]):
        return False
    return True

def _make_opts(
        copts = [],
        conlyopts = [],
        cxxopts = [],
        linkopts = []):
    """Creates new instrumentation options.

    The struct fields mirror the argument names of this function.

    Args:
      copts: A list of C/C++ compilation options passed as `--copt`
        configuration flags.
      conlyopts: A list of C-only compilation options passed as `--conlyopt`
        configuration flags.
      cxxopts: A list of C++-only compilation options passed as `--cxxopts`
        configuration flags.
      linkopts: A list of linker options to pass as `--linkopt`
        configuration flags.
    Returns:
      A struct with the given instrumentation options.
    """
    if not _is_string_list(copts):
        fail("copts should be a list of strings")
    if not _is_string_list(conlyopts):
        fail("conlyopts should be a list of strings")
    if not _is_string_list(cxxopts):
        fail("cxxopts should be a list of strings")
    if not _is_string_list(linkopts):
        fail("linkopts should be a list of strings")
    return struct(
        copts = copts,
        conlyopts = conlyopts,
        cxxopts = cxxopts,
        linkopts = linkopts,
    )

def _merge_opts(left_opts, right_opts):
    return _make_opts(
        copts = left_opts.copts + right_opts.copts,
        conlyopts = left_opts.conlyopts + right_opts.conlyopts,
        cxxopts = left_opts.cxxopts + right_opts.cxxopts,
        linkopts = left_opts.linkopts + right_opts.linkopts,
    )

instrum_opts = struct(
    make = _make_opts,
    merge = _merge_opts,
)

instrum_defaults = struct(
    # Instrumentation applied to all fuzz test executables when built in fuzzing
    # mode. This mode is controlled by the `//fuzzing:cc_fuzzing_build_mode`
    # config flag.
    fuzzing_build = _make_opts(
        copts = ["-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION"],
    ),
    libfuzzer = _make_opts(
        copts = ["-fsanitize=fuzzer-no-link"],
    ),
    # Jazzer is based on libFuzzer and hence generally requires the same
    # instrumentation for native code. Since it does not support
    # LeakSanitizer, the corresponding instrumentation can be disabled.
    jazzer = _make_opts(
        copts = [
            "-fsanitize=fuzzer-no-link",
            "-fno-sanitize=leak",
        ],
    ),
    # Reflects the set of options at
    # https://github.com/google/honggfuzz/blob/master/hfuzz_cc/hfuzz-cc.c
    honggfuzz = _make_opts(
        copts = [
            "-mllvm",
            "-inline-threshold=2000",
            "-fno-builtin",
            "-fno-omit-frame-pointer",
            "-D__NO_STRING_INLINES",
            "-fsanitize-coverage=trace-pc-guard,trace-cmp,trace-div,indirect-calls",
            "-fno-sanitize=fuzzer",
        ],
        linkopts = [
            "-fno-sanitize=fuzzer",
        ],
    ),
    asan = _make_opts(
        copts = ["-fsanitize=address"],
        linkopts = ["-fsanitize=address"],
    ),
    msan = _make_opts(
        copts = ["-fsanitize=memory"],
        linkopts = ["-fsanitize=memory"],
    ),
    msan_origin_tracking = _make_opts(
        copts = [
            "-fsanitize=memory",
            "-fsanitize-memory-track-origins=2",
        ],
        linkopts = ["-fsanitize=memory"],
    ),
    ubsan = _make_opts(
        copts = [
            "-fsanitize=undefined",
        ],
        linkopts = [
            "-fsanitize=undefined",
            # Bazel uses clang, not clang++, as the linker, which does not link
            # the C++ UBSan runtime library by default, but can be instructed to
            # do so with a flag.
            # https://github.com/bazelbuild/bazel/issues/11122#issuecomment-896613570
            "-fsanitize-link-c++-runtime",
        ],
    ),
)
