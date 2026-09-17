android_sdk_repository(
    name = "androidsdk",
    api_level = 28,
)

local_repository(
    name = "rules_jvm_external",
    path = "../../",
)

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    artifacts = [
        "junit:junit:4.12",
        "android.arch.lifecycle:common:1.1.1",
        "android.arch.lifecycle:viewmodel:1.1.1",
        "androidx.test.espresso:espresso-web:3.1.1",
        "com.android.support:design:27.0.2",
    ],
    generate_compat_repositories = True,
    repositories = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ],
)

load("@maven//:compat.bzl", "compat_repositories")

compat_repositories()
