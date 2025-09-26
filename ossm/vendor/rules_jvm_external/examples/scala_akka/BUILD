load("@io_bazel_rules_scala//scala:scala.bzl", "scala_binary", "scala_library", "scala_test_suite")

lib_dependencies = [
    "@maven//:com_chuusai_shapeless_2_12",
    "@maven//:com_github_pureconfig_pureconfig_2_12",
    "@maven//:com_github_pureconfig_pureconfig_core_2_12",
    "@maven//:com_github_pureconfig_pureconfig_generic_2_12",
    "@maven//:com_github_pureconfig_pureconfig_macros_2_12",
    "@maven//:com_typesafe_akka_akka_actor_2_12",
    "@maven//:com_typesafe_akka_akka_http_2_12",
    "@maven//:com_typesafe_akka_akka_http_core_2_12",
    "@maven//:com_typesafe_akka_akka_stream_2_12",
    "@maven//:com_typesafe_config",
    "@maven//:com_typesafe_scala_logging_scala_logging_2_12",
    "@maven//:org_scalaz_scalaz_core_2_12",
    "@maven//:org_scalaz_scalaz_concurrent_2_12",
    "@maven//:org_slf4j_slf4j_api",
    "@maven//:org_slf4j_slf4j_simple",
]

test_dependencies = [
    "@maven//:com_typesafe_akka_akka_testkit_2_12",
    "@maven//:com_typesafe_akka_akka_http_testkit_2_12",
    "@maven//:com_typesafe_akka_akka_stream_testkit_2_12",
    "@maven//:org_mockito_mockito_core",
    "@maven//:org_scalatest_scalatest_wordspec_2_12",
] + lib_dependencies

scala_library(
    name = "lib",
    srcs = glob([
        "src/main/scala/**/*.scala",
    ]),
    resources = glob([
        "src/main/resources/**/*",
    ]),
    scalacopts = [
        "-target:jvm-1.8",
        "-encoding",
        "UTF-8",
        "-Xfuture",
        "-Xlint",
        "-Xlint:unsound-match",
        "-Xmacro-settings:materialize-derivations",
        "-Yno-adapted-args",
        "-Ypartial-unification",
        "-Ywarn-unused-import",
        "-Ywarn-value-discard",
        "-Ywarn-dead-code",
        "-Ywarn-numeric-widen",
        "-language:existentials",
        "-language:experimental.macros",
        "-language:higherKinds",
        "-deprecation",
        "-feature",
        "-unchecked",
    ],
    deps = lib_dependencies,
)

scala_binary(
    name = "app",
    main_class = "hello.Application",
    runtime_deps = [
        ":lib",
    ],
)

scala_test_suite(
    name = "test",
    size = "small",
    srcs = glob([
        "src/test/**/*Spec.scala",
    ]),
    resources = glob([
        "src/test/resources/**/*",
    ]),
    deps = [
        ":lib",
    ] + test_dependencies,
)
