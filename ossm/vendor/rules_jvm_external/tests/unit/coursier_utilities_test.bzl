load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//private:coursier_utilities.bzl", "escape", "get_classifier", "get_packaging", "strip_packaging_and_classifier", "strip_packaging_and_classifier_and_version")

def _escape_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, "groupId_artifactId_1_2_3", escape("groupId:artifactId:1.2.3"))
    asserts.equals(env, "g_a_p_c_v", escape("g.a-p/c+v"))
    asserts.equals(env, "g_a_p_c_v", escape("g.[a-p]/c+v"))
    return unittest.end(env)

escape_test = unittest.make(_escape_test_impl)

def _get_classifier_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, None, get_classifier("groupId:artifactId:1.2.3"))
    asserts.equals(env, None, get_classifier("groupId:artifactId:packaging:1.2.3"))
    asserts.equals(env, "classifier", get_classifier("groupId:artifactId:packaging:classifier:1.2.3"))
    return unittest.end(env)

get_classifier_test = unittest.make(_get_classifier_test_impl)

def _get_packaging_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, None, get_packaging("groupId:artifactId:1.2.3"))
    asserts.equals(env, "packaging", get_packaging("groupId:artifactId:packaging:1.2.3"))
    asserts.equals(env, "packaging", get_packaging("groupId:artifactId:packaging:classifier:1.2.3"))
    asserts.equals(env, "packaging", get_packaging("groupId:artifactId:1.2.3@packaging"))
    asserts.equals(env, "packaging", get_packaging("groupId:artifactId:1.2.3:classifier@packaging"))
    return unittest.end(env)

get_packaging_test = unittest.make(_get_packaging_test_impl)

def _strip_packaging_and_classifier_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "groupId:artifactId:1.2.3",
        strip_packaging_and_classifier("groupId:artifactId:1.2.3"),
    )

    # Note: currently only some package and classifier values are stripped
    asserts.equals(
        env,
        "groupId:artifactId:packaging:1.2.3",
        strip_packaging_and_classifier("groupId:artifactId:packaging:1.2.3"),
    )
    asserts.equals(
        env,
        "groupId:artifactId:packaging:classifier:1.2.3",
        strip_packaging_and_classifier("groupId:artifactId:packaging:classifier:1.2.3"),
    )
    asserts.equals(
        env,
        "groupId:artifactId:1.2.3",
        strip_packaging_and_classifier("groupId:artifactId:bundle:1.2.3"),
    )
    asserts.equals(
        env,
        "groupId:artifactId:1.2.3",
        strip_packaging_and_classifier("groupId:artifactId:pom:sources:1.2.3"),
    )
    return unittest.end(env)

strip_packaging_and_classifier_test = unittest.make(_strip_packaging_and_classifier_test_impl)

def _strip_packaging_and_classifier_and_version_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "groupId:artifactId",
        strip_packaging_and_classifier_and_version("groupId:artifactId:1.2.3"),
    )

    # Note: currently only some package and classifier values are stripped
    asserts.equals(
        env,
        "groupId:artifactId",
        strip_packaging_and_classifier_and_version("groupId:artifactId:bundle:1.2.3"),
    )
    asserts.equals(
        env,
        "groupId:artifactId",
        strip_packaging_and_classifier_and_version("groupId:artifactId:pom:sources:1.2.3"),
    )

    # versionless coordinates aren't standard Maven coordinates but are useful for the artifact() macro
    asserts.equals(
        env,
        "groupId:artifactId",
        strip_packaging_and_classifier_and_version("groupId:artifactId"),
    )
    return unittest.end(env)

strip_packaging_and_classifier_and_version_test = unittest.make(_strip_packaging_and_classifier_and_version_test_impl)

def coursier_utilities_test_suite():
    unittest.suite(
        "coursier_utilities_tests",
        escape_test,
        get_classifier_test,
        get_packaging_test,
        strip_packaging_and_classifier_test,
        strip_packaging_and_classifier_and_version_test,
    )
