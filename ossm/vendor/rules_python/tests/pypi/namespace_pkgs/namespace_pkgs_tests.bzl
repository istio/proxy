""

load("@rules_testing//lib:analysis_test.bzl", "test_suite")
load("//python/private/pypi:namespace_pkgs.bzl", "create_inits", "get_files")  # buildifier: disable=bzl-visibility

_tests = []

def test_in_current_dir(env):
    srcs = [
        "foo/bar/biz.py",
        "foo/bee/boo.py",
        "foo/buu/__init__.py",
        "foo/buu/bii.py",
    ]
    got = get_files(srcs = srcs)
    expected = [
        "foo",
        "foo/bar",
        "foo/bee",
    ]
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_in_current_dir)

def test_find_correct_namespace_packages(env):
    srcs = [
        "nested/root/foo/bar/biz.py",
        "nested/root/foo/bee/boo.py",
        "nested/root/foo/buu/__init__.py",
        "nested/root/foo/buu/bii.py",
    ]

    got = get_files(srcs = srcs, root = "nested/root")
    expected = [
        "nested/root/foo",
        "nested/root/foo/bar",
        "nested/root/foo/bee",
    ]
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_find_correct_namespace_packages)

def test_ignores_empty_directories(_):
    # because globs do not add directories, this test is not needed
    pass

_tests.append(test_ignores_empty_directories)

def test_empty_case(env):
    srcs = [
        "foo/__init__.py",
        "foo/bar/__init__.py",
        "foo/bar/biz.py",
    ]

    got = get_files(srcs = srcs)
    expected = []
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_empty_case)

def test_ignores_non_module_files_in_directories(env):
    srcs = [
        "foo/__init__.pyi",
        "foo/py.typed",
    ]

    got = get_files(srcs = srcs)
    expected = []
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_ignores_non_module_files_in_directories)

def test_parent_child_relationship_of_namespace_pkgs(env):
    srcs = [
        "foo/bar/biff/my_module.py",
        "foo/bar/biff/another_module.py",
    ]

    got = get_files(srcs = srcs)
    expected = [
        "foo",
        "foo/bar",
        "foo/bar/biff",
    ]
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_parent_child_relationship_of_namespace_pkgs)

def test_parent_child_relationship_of_namespace_and_standard_pkgs(env):
    srcs = [
        "foo/bar/biff/__init__.py",
        "foo/bar/biff/another_module.py",
    ]

    got = get_files(srcs = srcs)
    expected = [
        "foo",
        "foo/bar",
    ]
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_parent_child_relationship_of_namespace_and_standard_pkgs)

def test_parent_child_relationship_of_namespace_and_nested_standard_pkgs(env):
    srcs = [
        "foo/bar/__init__.py",
        "foo/bar/biff/another_module.py",
        "foo/bar/biff/__init__.py",
        "foo/bar/boof/big_module.py",
        "foo/bar/boof/__init__.py",
        "fim/in_a_ns_pkg.py",
    ]

    got = get_files(srcs = srcs)
    expected = [
        "foo",
        "fim",
    ]
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_parent_child_relationship_of_namespace_and_nested_standard_pkgs)

def test_recognized_all_nonstandard_module_types(env):
    srcs = [
        "ayy/my_module.pyc",
        "bee/ccc/dee/eee.so",
        "eff/jee/aych.pyd",
    ]

    expected = [
        "ayy",
        "bee",
        "bee/ccc",
        "bee/ccc/dee",
        "eff",
        "eff/jee",
    ]
    got = get_files(srcs = srcs)
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_recognized_all_nonstandard_module_types)

def test_skips_ignored_directories(env):
    srcs = [
        "root/foo/boo/my_module.py",
        "root/foo/bar/another_module.py",
    ]

    expected = [
        "root/foo",
        "root/foo/bar",
    ]
    got = get_files(
        srcs = srcs,
        ignored_dirnames = ["root/foo/boo"],
        root = "root",
    )
    env.expect.that_collection(got).contains_exactly(expected)

_tests.append(test_skips_ignored_directories)

def _test_create_inits(env):
    srcs = [
        "nested/root/foo/bar/biz.py",
        "nested/root/foo/bee/boo.py",
        "nested/root/foo/buu/__init__.py",
        "nested/root/foo/buu/bii.py",
    ]
    copy_file_calls = []
    template = Label("//python/private/pypi:namespace_pkg_tmpl.py")

    got = create_inits(
        srcs = srcs,
        root = "nested/root",
        copy_file = lambda **kwargs: copy_file_calls.append(kwargs),
    )
    env.expect.that_collection(got).contains_exactly([
        call["out"]
        for call in copy_file_calls
    ])
    env.expect.that_collection(copy_file_calls).contains_exactly([
        {
            "name": "_cp_0_namespace",
            "out": "nested/root/foo/__init__.py",
            "src": template,
        },
        {
            "name": "_cp_1_namespace",
            "out": "nested/root/foo/bar/__init__.py",
            "src": template,
        },
        {
            "name": "_cp_2_namespace",
            "out": "nested/root/foo/bee/__init__.py",
            "src": template,
        },
    ])

_tests.append(_test_create_inits)

def namespace_pkgs_test_suite(name):
    test_suite(
        name = name,
        basic_tests = _tests,
    )
