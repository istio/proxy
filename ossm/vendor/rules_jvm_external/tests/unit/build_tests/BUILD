load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@bazel_skylib//rules:native_binary.bzl", "native_binary")

# Regression tests for particular artifacts that revealed problems for this
# project.

build_test(
    name = "all_artifacts",
    targets = [
        "@regression_testing_coursier//:build_buf_protovalidate",
        "@regression_testing_coursier//:ch_epfl_scala_compiler_interface",
        "@regression_testing_coursier//:com_fasterxml_jackson_jackson_bom",
        "@regression_testing_coursier//:com_github_fommil_netlib_all",
        "@regression_testing_coursier//:com_github_jnr_jffi_native",
        "@regression_testing_coursier//:com_google_protobuf_protoc_linux_x86_64",
        "@regression_testing_coursier//:com_google_protobuf_protoc_osx_aarch_64",
        "@regression_testing_coursier//:io_kubernetes_client_java",
        "@regression_testing_coursier//:io_netty_netty_tcnative_boringssl_static",
        "@regression_testing_coursier//:io_quarkus_http_quarkus_http_servlet",
        "@regression_testing_coursier//:nz_ac_waikato_cms_weka_weka_stable",
        "@regression_testing_coursier//:org_apache_flink_flink_test_utils_2_12",
        "@regression_testing_coursier//:org_eclipse_jetty_jetty_http_tests",
        "@regression_testing_coursier//:org_eclipse_jetty_orbit_javax_servlet",
        "@regression_testing_coursier//:org_junit_junit_bom",
        "@regression_testing_coursier//:org_openjfx_javafx_base",
        "@regression_testing_coursier//:org_pantsbuild_jarjar",
        "@regression_testing_maven//:org_apache_parquet_parquet_common",
    ],
)

# Note: skylib's `native_test` would fail on "Exec format error" under the other platform, hence the following 2 steps
[native_binary(
    name = "src_that_refers_to_a_valid_executable_{}_target".format(platform),
    src = "@regression_testing_coursier//:com_google_protobuf_protoc_{}".format(platform),
    out = "protoc_{}.exe".format(platform),
) for platform in ("linux_x86_64", "osx_aarch_64")]

build_test(
    name = "native_binaries_indeed_refer_to_valid_executable_targets",
    targets = [
        "src_that_refers_to_a_valid_executable_{}_target".format(platform)
        for platform in ("linux_x86_64", "osx_aarch_64")
    ],
)

build_test(
    name = "compat_repository_alias",
    targets = [
        "@org_pantsbuild_jarjar//jar",
        "@nz_ac_waikato_cms_weka_weka_stable//jar",
        "@org_eclipse_jetty_jetty_http_tests//jar",
        "@org_apache_flink_flink_test_utils_2_12//jar",
    ],
)

build_test(
    name = "compat_repository_alias_jarless",
    targets = [
        "@org_pantsbuild_jarjar",
        "@nz_ac_waikato_cms_weka_weka_stable",
        "@org_eclipse_jetty_jetty_http_tests",
        "@org_apache_flink_flink_test_utils_2_12",
    ],
)

build_test(
    name = "version_conflict_policy_pinned",
    targets = [
        "@policy_pinned_testing//:com_google_guava_guava_25_0_android",
    ],
)

build_test(
    name = "strict_version",
    targets = [
        "@strict_visibility_testing//:org_apache_tomcat_tomcat_catalina",
        "@strict_visibility_testing//:org_apache_tomcat_tomcat_catalina_9_0_24",
    ],
)

build_test(
    name = "strict_version_neg",
    tags = ["manual"],  # negative test case
    targets = [
        "@strict_visibility_testing//:org_apache_tomcat_tomcat_api",
    ],
)

build_test(
    name = "strict_version_classifier",
    targets = [
        "@strict_visibility_testing//:org_eclipse_jetty_jetty_http_tests",
    ],
)

build_test(
    name = "custom_maven_install_location",
    targets = [
        "@maven_install_in_custom_location//:com_google_guava_guava",
    ],
)

build_test(
    name = "compat_repository_alias_strict_version",
    targets = [
        "@strict_visibility_with_compat_testing//:com_google_http_client_google_http_client_gson",
        "@com_google_http_client_google_http_client_gson//jar",
        # Transitive compat alias will fail unless
        # @strict_visibility_with_compat_testing//:com_google_http_client_google_http_client is visible to it
        "@com_google_http_client_google_http_client//jar",
    ],
)
