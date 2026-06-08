# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Simple fuzz targets that demonstrate the Bazel extension functionality and
# serve as regression tests. Targets that are expected to crash or hang are
# disabled in the OSS-Fuzz integration using the "no-oss-fuzz" tag.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//fuzzing:java_defs.bzl", "java_fuzz_test")

filegroup(
    name = "corpus",
    srcs = [
        "corpus_0.txt",
        "corpus_1.txt",
    ],
)

java_fuzz_test(
    name = "EmptyFuzzTest",
    size = "small",
    srcs = ["com/example/EmptyFuzzTest.java"],
    corpus = [
        "corpus_0.txt",
    ],
)

java_fuzz_test(
    name = "FuzzTest",
    timeout = "short",
    srcs = ["com/example/FuzzTest.java"],
    tags = [
        "no-oss-fuzz",
    ],
)

java_fuzz_test(
    name = "NativeFuzzTest",
    srcs = ["com/example/NativeFuzzTest.java"],
    corpus = [
        ":corpus",
    ],
    tags = [
        "no-oss-fuzz",
    ],
    # The JVM expects a native library on macOS to have the .dylib extension,
    # but due to a bug in Bazel the shared library extension defaults to .so
    # there. This can be worked around by specifying the desired extension in
    # the name of the rule and selecting the correct one based on the platform.
    # See https://github.com/bazelbuild/bazel/issues/11082.
    deps = select({
        "@platforms//os:macos": [":libnative.dylib"],
        "//conditions:default": [":native"],
    }),
)

# A Java fuzz test with a native library, both of which have declared data
# dependencies that they can access at runtime.
java_fuzz_test(
    name = "NativeRunfileFuzzTest",
    srcs = ["com/example/NativeRunfileFuzzTest.java"],
    data = [
        "corpus_0.txt",
    ],
    # See NativeFuzzTest for why this uses a select.
    deps = select({
        "@platforms//os:macos": [":libnative_runfile.dylib"],
        "//conditions:default": [":native_runfile"],
    }) + [
        "@bazel_tools//tools/java/runfiles",
    ],
)

# A Java fuzz test with a native library that calls a function through a pointer
# of an incorrect type, which is detected by UBSan.
java_fuzz_test(
    name = "NativeUbsanFuncPtrFuzzTest",
    srcs = ["com/example/NativeUbsanFuncPtrFuzzTest.java"],
    deps = select({
        "@platforms//os:macos": [":libnative_ubsan_func_ptr.dylib"],
        "//conditions:default": [":native_ubsan_func_ptr"],
    }),
)

# A native library that interfaces with Java through the JNI.
# It contains an out-of-bounds read is detected by ASan.
cc_binary(
    name = "native",
    # Build as a shared library that can be loaded by a Java application at
    # runtime via System.loadLibrary().
    linkshared = True,
    tags = ["manual"],
    deps = [
        ":native_lib",
    ],
)

# The same shared library as :native, but with the correct extension for macOS.
# See the comment on :NativeFuzzTest for why this is needed.
cc_binary(
    name = "libnative.dylib",
    linkshared = True,
    tags = ["manual"],
    deps = [
        ":native_lib",
    ],
)

# The implementation shared by :native and :libnative.dylib, which differ only
# in the name of the resulting shared library.
cc_library(
    name = "native_lib",
    srcs = [
        "com/example/NativeFuzzTest.cpp",
        "com/example/NativeFuzzTest.h",
    ],
    deps = [
        "@bazel_tools//tools/jdk:jni",
    ],
    # Required because :native and :libnative.dylib to not reference any symbols
    # of this library, which means that it wouldn't be linked at all without
    # this.
    alwayslink = True,
)

# A shared library that demonstrates that fuzz targets can find their Bazel
# data dependencies at runtime, both from Java and native code.
cc_binary(
    name = "native_runfile",
    linkshared = True,
    tags = ["manual"],
    deps = [
        ":native_runfile_lib",
    ],
)

# The same shared library as :native_runfile, but with the correct extension for
# macOS. See the comment on :NativeFuzzTest for why this is needed.
cc_binary(
    name = "libnative_runfile.dylib",
    linkshared = True,
    tags = ["manual"],
    deps = [
        ":native_runfile_lib",
    ],
)

cc_library(
    name = "native_runfile_lib",
    srcs = [
        "com/example/NativeRunfileFuzzTest.cpp",
        "com/example/NativeRunfileFuzzTest.h",
    ],
    data = [
        "corpus_1.txt",
    ],
    deps = [
        "@bazel_tools//tools/cpp/runfiles",
        "@bazel_tools//tools/jdk:jni",
    ],
    alwayslink = True,
)

cc_binary(
    name = "native_ubsan_func_ptr",
    linkshared = True,
    tags = ["manual"],
    deps = [
        ":native_ubsan_func_ptr_lib",
    ],
)

cc_binary(
    name = "libnative_ubsan_func_ptr.dylib",
    linkshared = True,
    tags = ["manual"],
    deps = [
        ":native_ubsan_func_ptr_lib",
    ],
)

cc_library(
    name = "native_ubsan_func_ptr_lib",
    srcs = [
        "com/example/NativeUbsanFuncPtrFuzzTest.cpp",
        "com/example/NativeUbsanFuncPtrFuzzTest.h",
    ],
    deps = [
        "@bazel_tools//tools/jdk:jni",
    ],
    alwayslink = True,
)
