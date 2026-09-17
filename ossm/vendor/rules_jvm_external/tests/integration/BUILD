load("@aspect_bazel_lib//lib:diff_test.bzl", "diff_test")
load("@rules_java//java:java_library.bzl", "java_library")
load("//:defs.bzl", "artifact")
load(":is_bzlmod_enabled.bzl", "is_bzlmod_enabled")

genquery(
    name = "testonly-deps",
    testonly = True,
    expression = "attr(testonly, 1, deps(@testonly_testing//:all, 0))",
    scope = [
        "@testonly_testing//:org_codehaus_mojo_animal_sniffer_annotations_1_17",
        "@testonly_testing//:org_checkerframework_checker_qual_2_5_2",
        "@testonly_testing//:com_google_j2objc_j2objc_annotations_1_1",
        "@testonly_testing//:com_google_guava_listenablefuture_9999_0_empty_to_avoid_conflict_with_guava",
        "@testonly_testing//:com_google_guava_guava_27_0_jre",
        "@testonly_testing//:com_google_guava_guava",
        "@testonly_testing//:org_codehaus_mojo_animal_sniffer_annotations",
        "@testonly_testing//:org_checkerframework_checker_qual",
        "@testonly_testing//:com_google_j2objc_j2objc_annotations",
        "@testonly_testing//:com_google_guava_listenablefuture",
        "@testonly_testing//:com_google_guava_failureaccess_1_0",
        "@testonly_testing//:com_google_guava_failureaccess",
        "@testonly_testing//:com_google_errorprone_error_prone_annotations_2_2_0",
        "@testonly_testing//:com_google_errorprone_error_prone_annotations",
        "@testonly_testing//:com_google_code_findbugs_jsr305_3_0_2",
        "@testonly_testing//:com_google_code_findbugs_jsr305",
        "@testonly_testing//:com_google_auto_value_auto_value_annotations_1_6_3",
        "@testonly_testing//:com_google_auto_value_auto_value_annotations",
        "@testonly_testing//:defs",
        "@testonly_testing//:direct_deps",
        "@testonly_testing//:outdated",
        "@testonly_testing//:pin",
    ],
)

genrule(
    name = "testonly-deps-sorted",
    testonly = 1,
    srcs = [":testonly-deps"],
    outs = ["testonly-deps-sorted.txt"],
    cmd = "cat $< | sed -e 's|^@@|@|g; s|\r||g' | sed -e 's|^@[^/]*[+~]|@|g; s|\r||g' | sort > $@",
)

diff_test(
    name = "testonly_artifacts_test",
    file1 = "testonly-deps.golden.unix",
    file2 = ":testonly-deps-sorted",
)

# https://github.com/coursier/coursier/issues/1792
# https://github.com/bazelbuild/rules_jvm_external/issues/433
genquery(
    name = "version_interval_deps",
    testonly = True,
    expression = "deps(@version_interval_testing//:io_grpc_grpc_netty_shaded, 2)",
    opts = [
        "--nohost_deps",
        "--noimplicit_deps",
    ],
    scope = ["@version_interval_testing//:io_grpc_grpc_netty_shaded"],
)

genrule(
    name = "version-interval-deps-sorted",
    testonly = 1,
    srcs = [":version_interval_deps"],
    outs = ["version-interval-deps-sorted.txt"],
    cmd = "cat $< | sed -e 's|^@@|@|g; s|\r||g' | sed -e 's|^@[^/]*[+~]|@|g; s|\r||g' | sort > $@",
)

diff_test(
    name = "version_interval_deps_test",
    file1 = "version-interval-deps.golden.unix",
    file2 = ":version-interval-deps-sorted.txt",
)

genquery(
    name = "forced-versions-deps",
    expression = "deps(@forcing_versions//:xyz_rogfam_littleproxy)",
    opts = [
        "--nohost_deps",
        "--noimplicit_deps",
    ],
    scope = ["@forcing_versions//:xyz_rogfam_littleproxy"],
)

genrule(
    name = "forced-versions-deps-sorted",
    testonly = 1,
    srcs = [":forced-versions-deps"],
    outs = ["forced-versions-deps-sorted.txt"],
    cmd = "cat $< | sed -e 's|^@@|@|g; s|\r||g' | sed -e 's|^@[^/]*[+~]|@|g; s|\r||g' | sort > $@",
)

diff_test(
    name = "forced-version-deps-test",
    file1 = "forced-versions-deps.golden.unix",
    file2 = ":forced-versions-deps-sorted.txt",
)

genquery(
    name = "root-wins-deps",
    expression = "deps(@root_wins//:io_netty_netty_buffer)",
    opts = [
        "--nohost_deps",
        "--noimplicit_deps",
    ],
    scope = ["@root_wins//:io_netty_netty_buffer"],
)

genrule(
    name = "root-wins-deps-sorted",
    testonly = 1,
    srcs = [":root-wins-deps"],
    outs = ["root-wins-deps-sorted.txt"],
    cmd = "cat $< | grep root_wins | sed -e 's|^@@|@|g; s|\r||g' | sed -e 's|^@[^/]*[+~]|@|g; s|\r||g' | sort > $@",
)

# If the root module doesn't win, the version of `netty-buffer` will be `4.1.110.Final`
diff_test(
    name = "root-wins-deps-test",
    file1 = "root-wins-deps.golden.unix",
    file2 = ":root-wins-deps-sorted.txt",
    # This test only makes sense if we're running with `bzlmod` enabled
    tags = [] if is_bzlmod_enabled() else ["manual"],
)

# This target will fail to build if we're not handling merging of maven.install
# tags properly, and if we don't handle multiple lock files properly.
java_library(
    name = "demonstrate_multiple_lock_files_are_okay",
    exports = [
        # Dependency from our own `MODULE.bazel
        artifact(
            "org.zeromq:jeromq",
            repository_name = "multiple_lock_files",
        ),
        artifact(
            "redis.clients:jedis",
            repository_name = "multiple_lock_files",
        ),
    ],
)

genquery(
    name = "import-from-file-query",
    expression = "@from_files//:org_junit_jupiter_junit_jupiter_api",
    opts = [
        "--output=build",
    ],
    scope = ["@from_files//:org_junit_jupiter_junit_jupiter_api"],
)

genrule(
    name = "import-from-file-extract",
    srcs = [":import-from-file-query"],
    outs = ["import-from-file-extract.txt"],
    cmd = "cat $< | grep testonly >$@",
)

diff_test(
    name = "import-from-file",
    file1 = "import-from-file.unix",
    file2 = ":import-from-file-extract",
    # This test only makes sense if we're running with `bzlmod` enabled
    tags = [] if is_bzlmod_enabled() else ["manual"],
)

# Depend on something from the maven-resolved repo to ensure that things like the
# artifacts hash is consistent
genquery(
    name = "maven-resolved-is-ok",
    testonly = True,
    expression = "deps(@maven_resolved_with_boms//:org_seleniumhq_selenium_selenium_java)",
    opts = [
        "--nohost_deps",
        "--noimplicit_deps",
    ],
    scope = ["@maven_resolved_with_boms//:org_seleniumhq_selenium_selenium_java"],
    deps = [],
)
