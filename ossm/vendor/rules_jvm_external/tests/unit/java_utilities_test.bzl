load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//private:java_utilities.bzl", "build_java_argsfile_content")

def _build_java_argsfile_content_test_impl(ctx):
    env = unittest.begin(ctx)

    asserts.equals(
        env,
        """"--credentials"
"some.private.maven.re johndoe:example-password"
""",
        build_java_argsfile_content(["--credentials", "some.private.maven.re johndoe:example-password"]),
    )

    asserts.equals(
        env,
        """"--credentials"
"some.private.maven.re johndoe:example-password-with-\\"quotation-marks\\""
""",
        build_java_argsfile_content(["--credentials", "some.private.maven.re johndoe:example-password-with-\"quotation-marks\""]),
    )

    return unittest.end(env)

build_java_argsfile_content_test = unittest.make(_build_java_argsfile_content_test_impl)

def java_utilities_test_suite():
    unittest.suite(
        "java_utilities_tests",
        build_java_argsfile_content_test,
    )
