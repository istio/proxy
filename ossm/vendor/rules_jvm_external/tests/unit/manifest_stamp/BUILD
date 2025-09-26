load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@bazel_skylib//rules:diff_test.bzl", "diff_test")

build_test(
    name = "artifacts",
    targets = [
        # Test jar that does not have a META-INF/MANIFEST.MF file doesn't error when stamped
        "@manifest_stamp_testing//:javax_inject_javax_inject",
        # Test jar that has duplicate entries in the jar doesn't error when stamped
        "@manifest_stamp_testing//:org_apache_beam_beam_sdks_java_core",
    ],
)

genrule(
    name = "extract_manifest",
    srcs = [
        "@manifest_stamp_testing//:com_google_guava_guava",
    ],
    outs = [
        "guava_MANIFEST.MF",
    ],
    cmd = "$(location //tests/com/github/bazelbuild/rules_jvm_external/manifest:Print) $< | grep -E '^Bundle-Version|Target-Label' | sed -e 's|@@|@|g; s|\r||g' | sort > $@",
    toolchains = ["@bazel_tools//tools/jdk:current_host_java_runtime"],
    tools = [
        "//tests/com/github/bazelbuild/rules_jvm_external/manifest:Print",
    ],
)

diff_test(
    name = "diff_manifest_test",
    file1 = "guava_MANIFEST.MF.golden.unix",
    file2 = "guava_MANIFEST.MF",
)

genrule(
    name = "extract_signed_manifest",
    srcs = [
        "@manifest_stamp_testing//:org_bouncycastle_bcprov_jdk15on",
    ],
    outs = [
        "signed_MANIFEST.MF",
    ],
    cmd = "$(location //tests/com/github/bazelbuild/rules_jvm_external/manifest:Print) $< | grep -E '^Bundle-Version|Target-Label' | sed -e 's|@@|@|g; s|\r||g' | sort > $@",
    toolchains = ["@bazel_tools//tools/jdk:current_host_java_runtime"],
    tools = [
        "//tests/com/github/bazelbuild/rules_jvm_external/manifest:Print",
    ],
)

# Signed jars don't get a Target-Label because adding contents to the
# MANIFEST.MF file breaks the jar signature.
diff_test(
    name = "diff_signed_manifest_test",
    file1 = "bcprov_MANIFEST.MF.golden.unix",
    file2 = "signed_MANIFEST.MF",
)
