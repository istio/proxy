java_test(
    name = "HelloControllerTest",
    srcs = ["HelloControllerTest.java"],
    test_class = "hello.HelloControllerTest",
    deps = [
        "//src/main/java/hello:lib",
        "@maven//:org_hamcrest_hamcrest_library",
        "@maven//:org_springframework_boot_spring_boot_test",
        "@maven//:org_springframework_boot_spring_boot_test_autoconfigure",
        "@maven//:org_springframework_spring_beans",
        "@maven//:org_springframework_spring_test",
        "@maven//:org_springframework_spring_web",
    ],
)

java_test(
    name = "HelloControllerIntegrationTest",
    srcs = ["HelloControllerIT.java"],
    test_class = "hello.HelloControllerIT",
    deps = [
        "//src/main/java/hello:lib",
        "@maven//:org_hamcrest_hamcrest_library",
        "@maven//:org_springframework_boot_spring_boot",
        "@maven//:org_springframework_boot_spring_boot_test",
        "@maven//:org_springframework_spring_beans",
        "@maven//:org_springframework_spring_test",
        "@maven//:org_springframework_spring_web",
    ],
)
