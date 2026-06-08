load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//private/lib:urls.bzl", "extract_netrc_from_auth_url", "remove_auth_from_url", "split_url")
load(
    "//private/rules:coursier.bzl",
    "compute_dependency_inputs_signature",
    "get_coursier_cache_or_default",
    "get_coursier_sha256",
    "get_direct_dependencies",
    "get_netrc_lines_from_entries",
    infer = "infer_artifact_path_from_primary_and_repos",
)
load("//private/rules:v1_lock_file.bzl", "add_netrc_entries_from_mirror_urls")

ALL_TESTS = []

def add_test(test_impl_func):
    test = unittest.make(test_impl_func)
    ALL_TESTS.append(test)
    return test

def _infer_doc_example_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "group/path/to/artifact/file.jar",
        infer("http://a:b@c/group/path/to/artifact/file.jar", ["http://c"]),
    )
    return unittest.end(env)

infer_doc_example_test = add_test(_infer_doc_example_test_impl)

def _infer_basic_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "group/artifact/version/foo.jar",
        infer("https://base/group/artifact/version/foo.jar", ["https://base"]),
    )
    asserts.equals(
        env,
        "group/artifact/version/foo.jar",
        infer("http://base/group/artifact/version/foo.jar", ["http://base"]),
    )
    return unittest.end(env)

infer_basic_test = add_test(_infer_basic_test_impl)

def _infer_auth_basic_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "group1/artifact/version/foo.jar",
        infer("https://a@c/group1/artifact/version/foo.jar", ["https://a:b@c"]),
    )
    asserts.equals(
        env,
        "group2/artifact/version/foo.jar",
        infer("https://a@c/group2/artifact/version/foo.jar", ["https://a@c"]),
    )
    asserts.equals(
        env,
        "group3/artifact/version/foo.jar",
        infer("https://a@c/group3/artifact/version/foo.jar", ["https://c"]),
    )
    return unittest.end(env)

infer_auth_basic_test = add_test(_infer_auth_basic_test_impl)

def _infer_leading_repo_miss_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "group/artifact/version/foo.jar",
        infer("https://a@c/group/artifact/version/foo.jar", ["https://a:b@c/missubdir", "https://a:b@c"]),
    )
    return unittest.end(env)

infer_leading_repo_miss_test = add_test(_infer_leading_repo_miss_test_impl)

def _infer_repo_trailing_slash_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "group/artifact/version/foo.jar",
        infer("https://a@c/group/artifact/version/foo.jar", ["https://a:b@c"]),
    )
    asserts.equals(
        env,
        "group/artifact/version/foo.jar",
        infer("https://a@c/group/artifact/version/foo.jar", ["https://a:b@c/"]),
    )
    asserts.equals(
        env,
        "group/artifact/version/foo.jar",
        infer("https://a@c/group/artifact/version/foo.jar", ["https://a:b@c//"]),
    )
    return unittest.end(env)

infer_repo_trailing_slash_test = add_test(_infer_repo_trailing_slash_test_impl)

def _remove_auth_basic_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "https://c1",
        remove_auth_from_url("https://a:b@c1"),
    )
    return unittest.end(env)

remove_auth_basic_test = add_test(_remove_auth_basic_test_impl)

def _remove_auth_basic_with_path_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "https://c1/some/random/path",
        remove_auth_from_url("https://a:b@c1/some/random/path"),
    )
    return unittest.end(env)

remove_auth_basic_with_path_test = add_test(_remove_auth_basic_with_path_test_impl)

def _remove_auth_only_user_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "https://c1",
        remove_auth_from_url("https://a@c1"),
    )
    return unittest.end(env)

remove_auth_only_user_test = add_test(_remove_auth_only_user_test_impl)

def _remove_auth_noauth_noop_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "https://c1",
        remove_auth_from_url("https://c1"),
    )
    return unittest.end(env)

remove_auth_noauth_noop_test = add_test(_remove_auth_noauth_noop_test_impl)

def _split_url_basic_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        ("https", ["c1"]),
        split_url("https://c1"),
    )
    return unittest.end(env)

split_url_basic_test = add_test(_split_url_basic_test_impl)

def _split_url_basic_auth_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        ("https", ["a:b@c1"]),
        split_url("https://a:b@c1"),
    )
    asserts.equals(
        env,
        ("https", ["a@c1"]),
        split_url("https://a@c1"),
    )
    return unittest.end(env)

split_url_basic_auth_test = add_test(_split_url_basic_auth_test_impl)

def _split_url_with_path_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        ("https", ["c1", "some", "path"]),
        split_url("https://c1/some/path"),
    )
    return unittest.end(env)

split_url_with_path_test = add_test(_split_url_with_path_test_impl)

def _extract_netrc_from_auth_url_noop_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        {},
        extract_netrc_from_auth_url("https://c1"),
    )
    asserts.equals(
        env,
        {},
        extract_netrc_from_auth_url("https://c2/useless@inurl"),
    )
    return unittest.end(env)

extract_netrc_from_auth_url_noop_test = add_test(_extract_netrc_from_auth_url_noop_test_impl)

def _extract_netrc_from_auth_url_with_auth_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        {"machine": "c", "login": "a", "password": "b"},
        extract_netrc_from_auth_url("https://a:b@c"),
    )
    asserts.equals(
        env,
        {"machine": "c", "login": "a", "password": "b"},
        extract_netrc_from_auth_url("https://a:b@c/some/other/stuff@thisplace/for/testing"),
    )
    asserts.equals(
        env,
        {"machine": "c", "login": "a", "password": None},
        extract_netrc_from_auth_url("https://a@c"),
    )
    asserts.equals(
        env,
        {"machine": "c", "login": "a", "password": None},
        extract_netrc_from_auth_url("https://a@c/some/other/stuff@thisplace/for/testing"),
    )
    return unittest.end(env)

extract_netrc_from_auth_url_with_auth_test = add_test(_extract_netrc_from_auth_url_with_auth_test_impl)

def _extract_netrc_from_auth_url_at_in_password_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        {"machine": "c", "login": "a", "password": "p@ssword"},
        extract_netrc_from_auth_url("https://a:p@ssword@c"),
    )
    return unittest.end(env)

extract_netrc_from_auth_url_at_in_password_test = add_test(_extract_netrc_from_auth_url_at_in_password_test_impl)

def _add_netrc_entries_from_mirror_urls_noop_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        {},
        add_netrc_entries_from_mirror_urls({}, ["https://c1", "https://c1/something@there"]),
    )
    return unittest.end(env)

add_netrc_entries_from_mirror_urls_noop_test = add_test(_add_netrc_entries_from_mirror_urls_noop_test_impl)

def _add_netrc_entries_from_mirror_urls_basic_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        {"c1": {"a": "b"}},
        add_netrc_entries_from_mirror_urls({}, ["https://a:b@c1"]),
    )
    asserts.equals(
        env,
        {"c1": {"a": "b"}},
        add_netrc_entries_from_mirror_urls(
            {"c1": {"a": "b"}},
            ["https://a:b@c1"],
        ),
    )
    return unittest.end(env)

add_netrc_entries_from_mirror_urls_basic_test = add_test(_add_netrc_entries_from_mirror_urls_basic_test_impl)

def _add_netrc_entries_from_mirror_urls_multi_login_ignored_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        {"c1": {"a": "b"}},
        add_netrc_entries_from_mirror_urls({}, ["https://a:b@c1", "https://a:b2@c1", "https://a2:b3@c1"]),
    )
    asserts.equals(
        env,
        {"c1": {"a": "b"}},
        add_netrc_entries_from_mirror_urls(
            {"c1": {"a": "b"}},
            ["https://a:b@c1", "https://a:b2@c1", "https://a2:b3@c1"],
        ),
    )
    return unittest.end(env)

add_netrc_entries_from_mirror_urls_multi_login_ignored_test = add_test(_add_netrc_entries_from_mirror_urls_multi_login_ignored_test_impl)

def _add_netrc_entries_from_mirror_urls_multi_case_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        {
            "foo": {"bar": "baz"},
            "c1": {"a1": "b1"},
            "c2": {"a2": "b2"},
        },
        add_netrc_entries_from_mirror_urls(
            {"foo": {"bar": "baz"}},
            ["https://a1:b1@c1", "https://a2:b2@c2", "https://a:b@c1", "https://a:b2@c1", "https://a2:b3@c1"],
        ),
    )
    return unittest.end(env)

add_netrc_entries_from_mirror_urls_multi_case_test = add_test(_add_netrc_entries_from_mirror_urls_multi_case_test_impl)

def _get_netrc_lines_from_entries_noop_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        [],
        get_netrc_lines_from_entries({}),
    )
    return unittest.end(env)

get_netrc_lines_from_entries_noop_test = add_test(_get_netrc_lines_from_entries_noop_test_impl)

def _get_netrc_lines_from_entries_basic_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        ["machine c", "login a", "password b"],
        get_netrc_lines_from_entries({
            "c": {"a": "b"},
        }),
    )
    return unittest.end(env)

get_netrc_lines_from_entries_basic_test = add_test(_get_netrc_lines_from_entries_basic_test_impl)

def _get_netrc_lines_from_entries_no_pass_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        ["machine c", "login a"],
        get_netrc_lines_from_entries({
            "c": {"a": ""},
        }),
    )
    return unittest.end(env)

get_netrc_lines_from_entries_no_pass_test = add_test(_get_netrc_lines_from_entries_no_pass_test_impl)

def _get_netrc_lines_from_entries_multi_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        [
            "machine c",
            "login a",
            "password b",
            "machine c2",
            "login a2",
            "password p@ssword",
        ],
        get_netrc_lines_from_entries({
            "c": {"a": "b"},
            "c2": {"a2": "p@ssword"},
        }),
    )
    return unittest.end(env)

get_netrc_lines_from_entries_multi_test = add_test(_get_netrc_lines_from_entries_multi_test_impl)

def _mock_repo_path(path):
    if path.startswith("/"):
        return path
    else:
        return "/mockroot/" + path

def _mock_which(path):
    return False

def _get_coursier_cache_or_default_disabled_test(ctx):
    env = unittest.begin(ctx)
    mock_repository_ctx = struct(
        os = struct(
            environ = {
                "COURSIER_CACHE": _mock_repo_path("/does/not/matter"),
            },
            name = "linux",
        ),
        which = _mock_which,
    )
    asserts.equals(
        env,
        "v1",
        get_coursier_cache_or_default(mock_repository_ctx, False),
    )
    return unittest.end(env)

get_coursier_cache_or_default_disabled_test = add_test(_get_coursier_cache_or_default_disabled_test)

def _get_coursier_cache_or_default_enabled_with_default_location_linux_test(ctx):
    env = unittest.begin(ctx)
    mock_repository_ctx = struct(
        os = struct(
            environ = {
                "HOME": "/home/testuser",
            },
            name = "linux",
        ),
        which = _mock_which,
    )
    asserts.equals(
        env,
        "/home/testuser/.cache/coursier/v1",
        get_coursier_cache_or_default(mock_repository_ctx, True),
    )
    return unittest.end(env)

get_coursier_cache_or_default_enabled_with_default_location_linux_test = add_test(_get_coursier_cache_or_default_enabled_with_default_location_linux_test)

def _get_coursier_cache_or_default_enabled_with_default_location_mac_test(ctx):
    env = unittest.begin(ctx)
    mock_repository_ctx = struct(
        os = struct(
            environ = {
                "HOME": "/Users/testuser",
            },
            name = "mac",
        ),
        which = _mock_which,
    )
    asserts.equals(
        env,
        "/Users/testuser/Library/Caches/Coursier/v1",
        get_coursier_cache_or_default(mock_repository_ctx, True),
    )
    return unittest.end(env)

get_coursier_cache_or_default_enabled_with_default_location_mac_test = add_test(_get_coursier_cache_or_default_enabled_with_default_location_mac_test)

def _get_coursier_cache_or_default_enabled_with_custom_location_test(ctx):
    env = unittest.begin(ctx)
    mock_repository_ctx = struct(
        os = struct(
            environ = {
                "COURSIER_CACHE": _mock_repo_path("/custom/location"),
            },
            name = "linux",
        ),
        which = _mock_which,
    )
    asserts.equals(
        env,
        "/custom/location",
        get_coursier_cache_or_default(mock_repository_ctx, True),
    )
    return unittest.end(env)

get_coursier_cache_or_default_enabled_with_custom_location_test = add_test(_get_coursier_cache_or_default_enabled_with_custom_location_test)

def _get_coursier_sha256_default_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "default_sha256_value",
        get_coursier_sha256({}, "default_sha256_value"),
    )
    return unittest.end(env)

get_coursier_sha256_default_test = add_test(_get_coursier_sha256_default_test_impl)

def _get_coursier_sha256_from_env_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "custom_sha256_from_env",
        get_coursier_sha256({"COURSIER_SHA256": "custom_sha256_from_env"}, "default_sha256_value"),
    )
    return unittest.end(env)

get_coursier_sha256_from_env_test = add_test(_get_coursier_sha256_from_env_test_impl)

def _mock_which_true(path):
    return True

def _mock_execute(args):
    if args[-1] == "/Users/testuser/Library/Caches/Coursier/v1":
        return struct(return_code = 1)
    else:
        return struct(return_code = 0)

def _get_coursier_cache_or_default_enabled_with_home_dot_coursier_directory_test(ctx):
    env = unittest.begin(ctx)
    mock_repository_ctx = struct(
        os = struct(
            environ = {
                "HOME": "/Users/testuser",
            },
            name = "mac",
        ),
        which = _mock_which_true,
        execute = _mock_execute,
    )
    asserts.equals(
        env,
        "/Users/testuser/.coursier/cache/v1",
        get_coursier_cache_or_default(mock_repository_ctx, True),
    )
    return unittest.end(env)

get_coursier_cache_or_default_enabled_with_home_dot_coursier_directory_test = add_test(_get_coursier_cache_or_default_enabled_with_home_dot_coursier_directory_test)

def _calculate_inputs_hash_does_not_care_about_input_order_test(ctx):
    env = unittest.begin(ctx)

    boms1 = [
        """{"group": "first", "artifact": "bom", "version": "version"}""",
        """{"group": "second", "artifact": "bom", "version": "version"}""",
    ]
    artifacts1 = [
        """{"group": "first", "artifact": "artifact", "version": "version"}""",
        """{"group": "second", "artifact": "artifact", "version": "version"}""",
    ]
    repositories1 = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ]

    boms2 = [
        """{"group": "second", "artifact": "bom", "version": "version"}""",
        """{"group": "first", "artifact": "bom", "version": "version"}""",
    ]
    artifacts2 = [
        """{"group": "second", "artifact": "artifact", "version": "version"}""",
        """{"group": "first", "artifact": "artifact", "version": "version"}""",
    ]
    repositories2 = [
        "https://repo1.maven.org/maven2",
        "https://maven.google.com",
    ]

    # Order of artifacts is switched in each hash
    hash1, _ = compute_dependency_inputs_signature(
        boms = boms1,
        artifacts = artifacts1,
        repositories = repositories1,
        excluded_artifacts = artifacts2,
    )
    hash2, _ = compute_dependency_inputs_signature(
        boms = boms2,
        artifacts = artifacts2,
        repositories = repositories2,
        excluded_artifacts = artifacts1,
    )

    asserts.equals(env, hash1, hash2)

    return unittest.end(env)

calculate_inputs_hash_does_not_care_about_input_order_test = add_test(_calculate_inputs_hash_does_not_care_about_input_order_test)

def _calculate_inputs_hash_is_different_for_different_repositories_test(ctx):
    env = unittest.begin(ctx)

    boms1 = [
        """{"group": "first", "artifact": "bom", "version": "version"}""",
        """{"group": "second", "artifact": "bom", "version": "version"}""",
    ]
    artifacts1 = [
        """{"group": "first", "artifact": "artifact", "version": "version"}""",
        """{"group": "second", "artifact": "artifact", "version": "version"}""",
    ]
    repositories1 = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ]

    boms2 = [
        """{"group": "second", "artifact": "bom", "version": "version"}""",
        """{"group": "first", "artifact": "bom", "version": "version"}""",
    ]
    artifacts2 = [
        """{"group": "second", "artifact": "artifact", "version": "version"}""",
        """{"group": "first", "artifact": "artifact", "version": "version"}""",
    ]
    repositories2 = [
        "https://repo1.maven.org/maven2",
    ]

    # Order of artifacts is switched in each hash
    hash1, _ = compute_dependency_inputs_signature(
        boms = boms1,
        artifacts = artifacts1,
        repositories = repositories1,
        excluded_artifacts = [],
    )
    hash2, _ = compute_dependency_inputs_signature(
        boms = boms2,
        artifacts = artifacts2,
        repositories = repositories2,
        excluded_artifacts = [],
    )

    asserts.false(env, hash1 == hash2)

    return unittest.end(env)

calculate_inputs_hash_is_different_for_different_repositories_test = add_test(_calculate_inputs_hash_is_different_for_different_repositories_test)

def _calculate_inputs_hash_uses_excluded_artifacts_test(ctx):
    env = unittest.begin(ctx)

    boms1 = [
        """{"group": "first", "artifact": "bom", "version": "version"}""",
        """{"group": "second", "artifact": "bom", "version": "version"}""",
    ]
    artifacts1 = [
        """{"group": "first", "artifact": "artifact", "version": "version"}""",
        """{"group": "second", "artifact": "artifact", "version": "version"}""",
    ]
    repositories1 = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ]

    boms2 = [
        """{"group": "second", "artifact": "bom", "version": "version"}""",
        """{"group": "first", "artifact": "bom", "version": "version"}""",
    ]
    artifacts2 = [
        """{"group": "second", "artifact": "artifact", "version": "version"}""",
        """{"group": "first", "artifact": "artifact", "version": "version"}""",
    ]

    excluded1 = ["""{"group": "first", "artifact": "artifact", "version": "version1"}"""]
    excluded2 = ["""{"group": "first", "artifact": "artifact", "version": "version2"}"""]
    hash1, old_hashes1 = compute_dependency_inputs_signature(
        boms = boms1,
        artifacts = artifacts1,
        repositories = repositories1,
        excluded_artifacts = excluded1,
    )
    hash2, old_hashes2 = compute_dependency_inputs_signature(
        boms = boms2,
        artifacts = artifacts1,
        repositories = repositories1,
        excluded_artifacts = excluded2,
    )

    asserts.false(env, hash1 == hash2)
    asserts.true(env, old_hashes1[0] == old_hashes2[0])
    asserts.false(env, old_hashes1[1] == old_hashes2[1])

    return unittest.end(env)

calculate_inputs_hash_uses_excluded_artifacts_test = add_test(_calculate_inputs_hash_uses_excluded_artifacts_test)

def _get_direct_dependencies_test_impl(ctx):
    env = unittest.begin(ctx)

    all_artifacts = [
        {"coordinates": "com.google.guava:guava:31.0-jre"},
        {"coordinates": "com.google.code.gson:gson:2.10.1"},
        {"coordinates": "junit:junit:4.13.2"},
        {"coordinates": "io.netty:netty-tcnative:2.0.61.Final"},
        {"coordinates": "io.netty:netty-tcnative:2.0.61.Final:linux-x86_64"},
        {"coordinates": "com.example:lib:1.0@aar"},
    ]

    input_artifacts = [
        # Basic: should resolve to 31.0-jre even though 30.0-jre was requested
        {"group": "com.google.guava", "artifact": "guava", "version": "30.0-jre"},
        # Duplicate: same artifact requested again, should be deduplicated
        {"group": "com.google.guava", "artifact": "guava", "version": "29.0-jre"},
        # With classifier: should match the linux-x86_64 variant
        {"group": "io.netty", "artifact": "netty-tcnative", "version": "2.0.60.Final", "classifier": "linux-x86_64"},
        # With packaging: should match the aar
        {"group": "com.example", "artifact": "lib", "version": "1.0", "packaging": "aar"},
        # Missing: should be skipped
        {"group": "com.example", "artifact": "not-present", "version": "1.0"},
    ]

    result = get_direct_dependencies(all_artifacts, input_artifacts)

    # Should have 3 results: guava (deduplicated), netty with classifier, and aar
    asserts.equals(env, 3, len(result))
    asserts.true(env, "com.google.guava:guava:31.0-jre" in result)
    asserts.true(env, "io.netty:netty-tcnative:2.0.61.Final:linux-x86_64" in result)
    asserts.true(env, "com.example:lib:1.0@aar" in result)

    return unittest.end(env)

get_direct_dependencies_test = add_test(_get_direct_dependencies_test_impl)

def coursier_test_suite():
    unittest.suite(
        "coursier_tests",
        *ALL_TESTS
    )
