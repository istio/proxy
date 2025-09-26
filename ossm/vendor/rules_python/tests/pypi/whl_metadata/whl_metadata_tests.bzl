""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "subjects")
load(
    "//python/private/pypi:whl_metadata.bzl",
    "find_whl_metadata",
    "parse_whl_metadata",
)  # buildifier: disable=bzl-visibility

_tests = []

def _test_empty(env):
    fake_path = struct(
        basename = "site-packages",
        readdir = lambda watch = None: [],
    )
    fail_messages = []
    find_whl_metadata(install_dir = fake_path, logger = struct(
        fail = fail_messages.append,
    ))
    env.expect.that_collection(fail_messages).contains_exactly([
        "The '*.dist-info' directory could not be found in 'site-packages'",
    ])

_tests.append(_test_empty)

def _test_contains_dist_info_but_no_metadata(env):
    fake_path = struct(
        basename = "site-packages",
        readdir = lambda watch = None: [
            struct(
                basename = "something.dist-info",
                is_dir = True,
                get_child = lambda basename: struct(
                    basename = basename,
                    exists = False,
                ),
            ),
        ],
    )
    fail_messages = []
    find_whl_metadata(install_dir = fake_path, logger = struct(
        fail = fail_messages.append,
    ))
    env.expect.that_collection(fail_messages).contains_exactly([
        "The METADATA file for the wheel could not be found in 'site-packages/something.dist-info'",
    ])

_tests.append(_test_contains_dist_info_but_no_metadata)

def _test_contains_metadata(env):
    fake_path = struct(
        basename = "site-packages",
        readdir = lambda watch = None: [
            struct(
                basename = "something.dist-info",
                is_dir = True,
                get_child = lambda basename: struct(
                    basename = basename,
                    exists = True,
                ),
            ),
        ],
    )
    fail_messages = []
    got = find_whl_metadata(install_dir = fake_path, logger = struct(
        fail = fail_messages.append,
    ))
    env.expect.that_collection(fail_messages).contains_exactly([])
    env.expect.that_str(got.basename).equals("METADATA")

_tests.append(_test_contains_metadata)

def _parse_whl_metadata(env, **kwargs):
    result = parse_whl_metadata(**kwargs)

    return env.expect.that_struct(
        struct(
            name = result.name,
            version = result.version,
            requires_dist = result.requires_dist,
            provides_extra = result.provides_extra,
        ),
        attrs = dict(
            name = subjects.str,
            version = subjects.str,
            requires_dist = subjects.collection,
            provides_extra = subjects.collection,
        ),
    )

def _test_parse_metadata_invalid(env):
    got = _parse_whl_metadata(
        env,
        contents = "",
    )
    got.name().equals("")
    got.version().equals("")
    got.requires_dist().contains_exactly([])
    got.provides_extra().contains_exactly([])

_tests.append(_test_parse_metadata_invalid)

def _test_parse_metadata_basic(env):
    got = _parse_whl_metadata(
        env,
        contents = """\
Name: foo
Version: 0.0.1
""",
    )
    got.name().equals("foo")
    got.version().equals("0.0.1")
    got.requires_dist().contains_exactly([])
    got.provides_extra().contains_exactly([])

_tests.append(_test_parse_metadata_basic)

def _test_parse_metadata_all(env):
    got = _parse_whl_metadata(
        env,
        contents = """\
Name: foo
Version: 0.0.1
Requires-Dist: bar; extra == "all"
Provides-Extra: all

Requires-Dist: this will be ignored
""",
    )
    got.name().equals("foo")
    got.version().equals("0.0.1")
    got.requires_dist().contains_exactly([
        "bar; extra == \"all\"",
    ])
    got.provides_extra().contains_exactly([
        "all",
    ])

_tests.append(_test_parse_metadata_all)

def _test_parse_metadata_multiline_license(env):
    got = _parse_whl_metadata(
        env,
        # NOTE: The trailing whitespace here is meaningful as an empty line
        # denotes the end of the header.
        contents = """\
Name: foo
Version: 0.0.1
License: some License
        
        some line
        
        another line
        
Requires-Dist: bar; extra == "all"
Provides-Extra: all

Requires-Dist: this will be ignored
""",
    )
    got.name().equals("foo")
    got.version().equals("0.0.1")
    got.requires_dist().contains_exactly([
        "bar; extra == \"all\"",
    ])
    got.provides_extra().contains_exactly([
        "all",
    ])

_tests.append(_test_parse_metadata_multiline_license)

def whl_metadata_test_suite(name):  # buildifier: disable=function-docstring
    test_suite(
        name = name,
        basic_tests = _tests,
    )
