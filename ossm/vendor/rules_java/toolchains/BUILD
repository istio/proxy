load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    ":default_java_toolchain.bzl",
    "DEFAULT_TOOLCHAIN_CONFIGURATION",
    "PREBUILT_TOOLCHAIN_CONFIGURATION",
    "bootclasspath",
    "default_java_toolchain",
    "java_runtime_files",
)
load(
    ":java_toolchain_alias.bzl",
    "java_host_runtime_alias",
    "java_runtime_alias",
    "java_runtime_version_alias",
    "java_toolchain_alias",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

filegroup(
    name = "bzl_srcs",
    srcs = glob(["*.bzl"]),
)

# A single binary distribution of a JDK (e.g., OpenJDK 17 for Windows arm64) provides three
# different types of toolchains from the perspective of Bazel:

# The compilation toolchain, which provides the Java runtime used to execute the Java compiler, as
# well as various helper tools and settings.
#
# Toolchains of this type typically have constraints on the execution platform so that their Java
# runtime can run the compiler, but not on the target platform as Java compilation outputs are
# platform independent.
#
# Obtain the associated JavaToolchainInfo via:
#   ctx.toolchains["@bazel_tools//tools/jdk:toolchain_type"].java
# TODO: migrate away from using @bazel_tools//tools/jdk:toolchain_type ?
# toolchain_type(name = "toolchain_type")

# The Java runtime that executable Java compilation outputs (e.g., java_binary with
# create_executable = True) will run on.
#
# Toolchains of this type typically have constraints on the target platform so that the runtime's
# native 'java' binary can be run there, but not on the execution platform as building an executable
# Java target only requires copying or symlinking the runtime, which can be done on any platform.
#
# Obtain the associated JavaRuntimeInfo via:
#   ctx.toolchains["@bazel_tools//tools/jdk:runtime_toolchain_type"].java_runtime
# TODO: migrate away from using @bazel_tools//tools/jdk:runtime_toolchain_type ?
# toolchain_type(name = "runtime_toolchain_type")

# The Java runtime to extract the bootclasspath from that is then used to compile Java sources.
#
# As the bootclasspath is platform independent, toolchains of this type may have no constraints.
# Purely as an optimization to prevent unnecessary fetches of remote runtimes for other
# architectures, toolchains of this type may have constraints on the execution platform that match
# those on the corresponding compilation toolchain.
#
# Toolchains of this type are only consumed internally by the bootclasspath rule and should not be
# accessed from Starlark.
# TODO: migrate away from using @bazel_tools//tools/jdk:bootstrap_runtime_toolchain_type ?
# toolchain_type(name = "bootstrap_runtime_toolchain_type")

# Points to toolchain[":runtime_toolchain_type"] (was :legacy_current_java_runtime)
java_runtime_alias(name = "current_java_runtime")

# Host configuration of ":current_java_runtime"
java_host_runtime_alias(name = "current_host_java_runtime")

# Points to toolchain[":toolchain_type"] (was :legacy_current_java_toolchain)
java_toolchain_alias(name = "current_java_toolchain")

# These individual jni_* targets are exposed for legacy reasons.
# Most users should depend on :jni.

java_runtime_files(
    name = "jni_header",
    srcs = ["include/jni.h"],
)

java_runtime_files(
    name = "jni_md_header-darwin",
    srcs = ["include/darwin/jni_md.h"],
)

java_runtime_files(
    name = "jni_md_header-linux",
    srcs = ["include/linux/jni_md.h"],
)

java_runtime_files(
    name = "jni_md_header-windows",
    srcs = ["include/win32/jni_md.h"],
)

java_runtime_files(
    name = "jni_md_header-freebsd",
    srcs = ["include/freebsd/jni_md.h"],
)

java_runtime_files(
    name = "jni_md_header-openbsd",
    srcs = ["include/openbsd/jni_md.h"],
)

# The Java native interface. Depend on this package if you #include <jni.h>.
#
# See test_jni in third_party/bazel/src/test/shell/bazel/bazel_java_test.sh for
# an example of using Bazel to build a Java program that calls a C function.
#
# TODO(ilist): use //src:condition:linux when released in Bazel
cc_library(
    name = "jni",
    hdrs = [":jni_header"] + select({
        "@bazel_tools//src/conditions:darwin": [":jni_md_header-darwin"],
        "@bazel_tools//src/conditions:freebsd": [":jni_md_header-freebsd"],
        "@bazel_tools//src/conditions:linux_aarch64": [":jni_md_header-linux"],
        "@bazel_tools//src/conditions:linux_mips64": [":jni_md_header-linux"],
        "@bazel_tools//src/conditions:linux_ppc64le": [":jni_md_header-linux"],
        "@bazel_tools//src/conditions:linux_riscv64": [":jni_md_header-linux"],
        "@bazel_tools//src/conditions:linux_s390x": [":jni_md_header-linux"],
        "@bazel_tools//src/conditions:linux_x86_64": [":jni_md_header-linux"],
        "@bazel_tools//src/conditions:openbsd": [":jni_md_header-openbsd"],
        "@bazel_tools//src/conditions:windows": [":jni_md_header-windows"],
        "//conditions:default": [],
    }),
    includes = ["include"] + select({
        "@bazel_tools//src/conditions:darwin": ["include/darwin"],
        "@bazel_tools//src/conditions:freebsd": ["include/freebsd"],
        "@bazel_tools//src/conditions:linux_aarch64": ["include/linux"],
        "@bazel_tools//src/conditions:linux_mips64": ["include/linux"],
        "@bazel_tools//src/conditions:linux_ppc64le": ["include/linux"],
        "@bazel_tools//src/conditions:linux_riscv64": ["include/linux"],
        "@bazel_tools//src/conditions:linux_s390x": ["include/linux"],
        "@bazel_tools//src/conditions:linux_x86_64": ["include/linux"],
        "@bazel_tools//src/conditions:openbsd": ["include/openbsd"],
        "@bazel_tools//src/conditions:windows": ["include/win32"],
        "//conditions:default": [],
    }),
    tags = ["nobuilder"],
)

[
    (
        alias(
            name = "ijar_prebuilt_binary_%s" % OS,
            actual = "@remote_java_tools_%s//:ijar_prebuilt_binary" % OS,
            visibility = ["//visibility:private"],
        ),
        alias(
            name = "prebuilt_singlejar_%s" % OS,
            actual = "@remote_java_tools_%s//:prebuilt_singlejar" % OS,
            visibility = ["//visibility:private"],
        ),
        alias(
            name = "prebuilt_one_version_%s" % OS,
            actual = "@remote_java_tools_%s//:prebuilt_one_version" % OS,
            visibility = ["//visibility:private"],
        ),
        alias(
            name = "turbine_direct_graal_%s" % OS,
            actual = "@remote_java_tools_%s//:turbine_direct_graal" % OS,
        ),
    )
    for OS in [
        "linux",
        "darwin_x86_64",
        "darwin_arm64",
        "windows",
    ]
]

alias(
    name = "ijar",
    actual = ":ijar_prebuilt_binary_or_cc_binary",
)

alias(
    name = "ijar_prebuilt_binary_or_cc_binary",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":ijar_prebuilt_binary_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":ijar_prebuilt_binary_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":ijar_prebuilt_binary_linux",
        "@bazel_tools//src/conditions:windows": ":ijar_prebuilt_binary_windows",
        "//conditions:default": "@remote_java_tools//:ijar_cc_binary",
    }),
)

alias(
    name = "ijar_prebuilt_binary",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":ijar_prebuilt_binary_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":ijar_prebuilt_binary_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":ijar_prebuilt_binary_linux",
        "@bazel_tools//src/conditions:windows": ":ijar_prebuilt_binary_windows",
    }),
)

alias(
    name = "singlejar",
    actual = ":singlejar_prebuilt_or_cc_binary",
)

alias(
    name = "singlejar_prebuilt_or_cc_binary",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":prebuilt_singlejar_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":prebuilt_singlejar_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":prebuilt_singlejar_linux",
        "@bazel_tools//src/conditions:windows": ":prebuilt_singlejar_windows",
        "//conditions:default": "@remote_java_tools//:singlejar_cc_bin",
    }),
)

alias(
    name = "prebuilt_singlejar",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":prebuilt_singlejar_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":prebuilt_singlejar_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":prebuilt_singlejar_linux",
        "@bazel_tools//src/conditions:windows": ":prebuilt_singlejar_windows",
    }),
)

alias(
    name = "one_version",
    actual = ":one_version_prebuilt_or_cc_binary",
)

alias(
    name = "one_version_prebuilt_or_cc_binary",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":prebuilt_one_version_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":prebuilt_one_version_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":prebuilt_one_version_linux",
        "@bazel_tools//src/conditions:windows": ":prebuilt_one_version_windows",
        "//conditions:default": "@remote_java_tools//:one_version_cc_bin",
    }),
)

alias(
    name = "prebuilt_one_version",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":prebuilt_one_version_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":prebuilt_one_version_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":prebuilt_one_version_linux",
        "@bazel_tools//src/conditions:windows": ":prebuilt_one_version_windows",
    }),
)

alias(
    name = "turbine_direct",
    actual = ":turbine_direct_graal_or_java",
)

alias(
    name = "turbine_direct_graal_or_java",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":turbine_direct_graal_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":turbine_direct_graal_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":turbine_direct_graal_linux",
        "@bazel_tools//src/conditions:windows": ":turbine_direct_graal_windows",
        "//conditions:default": "@remote_java_tools//:TurbineDirect",
    }),
)

alias(
    name = "turbine_direct_graal",
    actual = select({
        "@bazel_tools//src/conditions:darwin_arm64": ":turbine_direct_graal_darwin_arm64",
        "@bazel_tools//src/conditions:darwin_x86_64": ":turbine_direct_graal_darwin_x86_64",
        "@bazel_tools//src/conditions:linux_x86_64": ":turbine_direct_graal_linux",
        "@bazel_tools//src/conditions:windows": ":turbine_direct_graal_windows",
    }),
)

bootclasspath(
    name = "platformclasspath",
    src = "DumpPlatformClassPath.java",
    java_runtime_alias = ":current_java_runtime",
)

default_java_toolchain(
    name = "toolchain",
    configuration = DEFAULT_TOOLCHAIN_CONFIGURATION,
    toolchain_definition = False,
)

alias(
    name = "remote_toolchain",
    actual = ":toolchain",
)

RELEASES = (8, 9, 10, 11, 17, 21)

[
    default_java_toolchain(
        name = ("toolchain_java%d" if release <= 11 else "toolchain_jdk_%d") % release,
        configuration = DEFAULT_TOOLCHAIN_CONFIGURATION,
        source_version = "%s" % release,
        target_version = "%s" % release,
    )
    for release in RELEASES
]

default_java_toolchain(
    name = "prebuilt_toolchain",
    configuration = PREBUILT_TOOLCHAIN_CONFIGURATION,
    toolchain_definition = False,
)

# A JDK 11 for use as a --host_javabase.
java_runtime_version_alias(
    name = "remote_jdk11",
    runtime_version = "remotejdk_11",
    visibility = ["//visibility:public"],
)

java_runtime_version_alias(
    name = "remotejdk_17",
    runtime_version = "remotejdk_17",
    visibility = ["//visibility:public"],
)

java_runtime_version_alias(
    name = "remotejdk_21",
    runtime_version = "remotejdk_21",
    visibility = ["//visibility:public"],
)

java_runtime_version_alias(
    name = "jdk_8",
    runtime_version = "8",
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "toolchain_utils",
    srcs = ["toolchain_utils.bzl"],
    visibility = ["//visibility:public"],
    deps = ["//java/common"],
)
