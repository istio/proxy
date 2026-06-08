""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python:py_info.bzl", "PyInfo")
load("//python:py_library.bzl", "py_library")
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
load("//python/private:py_info.bzl", "VenvSymlinkEntry", "VenvSymlinkKind")  # buildifier: disable=bzl-visibility
load("//python/private:venv_runfiles.bzl", "build_link_map", "get_venv_symlinks")  # buildifier: disable=bzl-visibility
load("//tests/support:support.bzl", "SUPPORTS_BZLMOD_UNIXY")

def _empty_files_impl(ctx):
    files = []
    for p in ctx.attr.paths:
        f = ctx.actions.declare_file(p)
        ctx.actions.write(output = f, content = "")
        files.append(f)
    return [DefaultInfo(files = depset(files))]

empty_files = rule(
    implementation = _empty_files_impl,
    attrs = {
        "paths": attr.string_list(
            doc = "A list of paths to create as files.",
            mandatory = True,
        ),
    },
)

_tests = []

# NOTE: In bzlmod, the workspace name is always "_main".
# Under workspace, the workspace name is the name configured in WORKSPACE,
# or "__main__" if was unspecified.
# NOTE: ctx.workspace_name is always the root workspace, not the workspace
# of the target being processed (ctx.label).
def _ctx(workspace_name = "_main"):
    return struct(
        workspace_name = workspace_name,
        label = Label("@@FAKE-CTX//:fake_ctx"),
    )

def _file(short_path):
    return struct(
        short_path = short_path,
    )

def _venv_symlink(venv_path, *, link_to_path = None, files = []):
    return struct(
        link_to_path = link_to_path,
        venv_path = venv_path,
        files = files,
    )

def _venv_symlinks_from_entries(entries):
    result = []
    for symlink_entry in entries:
        result.append(struct(
            venv_path = symlink_entry.venv_path,
            link_to_path = symlink_entry.link_to_path,
            files = [f.short_path for f in symlink_entry.files.to_list()],
        ))
    return sorted(result, key = lambda e: (e.link_to_path, e.venv_path))

def _entry(venv_path, link_to_path, files, **kwargs):
    kwargs.setdefault("kind", VenvSymlinkKind.LIB)
    kwargs.setdefault("package", None)
    kwargs.setdefault("version", None)
    kwargs.setdefault("link_to_file", None)

    return VenvSymlinkEntry(
        venv_path = venv_path,
        link_to_path = link_to_path,
        files = depset(files),
        **kwargs
    )

def _test_conflict_merging(name):
    analysis_test(
        name = name,
        impl = _test_conflict_merging_impl,
        target = "//python:none",
    )

_tests.append(_test_conflict_merging)

def _test_conflict_merging_impl(env, _):
    entries = [
        _entry("a", "+pypi_a/site-packages/a", [
            _file("../+pypi_a/site-packages/a/a.txt"),
        ]),
        _entry("a-1.0.dist-info", "+pypi_a/site-packages/a-1.0.dist-info", [
            _file("../+pypi_a/site-packages/a-1.0.dist-info/METADATA"),
        ]),
        _entry("a/b", "+pypi_a_b/site-packages/a/b", [
            _file("../+pypi_a_b/site-packages/a/b/b.txt"),
        ]),
        _entry("x", "_main/src/x", [
            _file("src/x/x.txt"),
        ]),
        _entry("x/p", "_main/src-dev/x/p", [
            _file("src-dev/x/p/p.txt"),
        ]),
        _entry("duplicate", "+dupe_a/site-packages/duplicate", [
            _file("../+dupe_a/site-packages/duplicate/d.py"),
        ]),
        _entry("duplicate", "+dupe_b/site-packages/duplicate", [
            _file("../+dupe_b/site-packages/duplicate/d.py"),
        ]),
        # Case: two distributions provide the same file (instead of directory)
        _entry("ff/fmod.py", "+ff_a/site-packages/ff/fmod.py", [
            _file("../+ff_a/site-packages/ff/fmod.py"),
        ]),
        _entry("ff/fmod.py", "+ff_b/site-packages/ff/fmod.py", [
            _file("../+ff_b/site-packages/ff/fmod.py"),
        ]),
    ]

    actual, conflicts = build_link_map(_ctx(), entries, return_conflicts = True)
    expected_libs = {
        "a-1.0.dist-info": "+pypi_a/site-packages/a-1.0.dist-info",
        "a/a.txt": _file("../+pypi_a/site-packages/a/a.txt"),
        "a/b/b.txt": _file("../+pypi_a_b/site-packages/a/b/b.txt"),
        "duplicate/d.py": _file("../+dupe_a/site-packages/duplicate/d.py"),
        "ff/fmod.py": _file("../+ff_a/site-packages/ff/fmod.py"),
        "x/p/p.txt": _file("src-dev/x/p/p.txt"),
        "x/x.txt": _file("src/x/x.txt"),
    }
    env.expect.that_dict(actual[VenvSymlinkKind.LIB]).contains_exactly(expected_libs)
    env.expect.that_dict(actual).keys().contains_exactly([VenvSymlinkKind.LIB])

    env.expect.that_int(len(conflicts)).is_greater_than(0)

def _test_optimized_grouping_complex(name):
    empty_files(
        name = name + "_files",
        paths = [
            "site-packages/pkg1/a.txt",
            "site-packages/pkg1/b/b_mod_so",
            "site-packages/pkg1/c/c1.txt",
            "site-packages/pkg1/c/c2.txt",
            "site-packages/pkg1/d/d1.txt",
            "site-packages/pkg1/dd/dd1.txt",
            "site-packages/pkg1/q1/q1.txt",
            "site-packages/pkg1/q1/q2a/libq.so",
            "site-packages/pkg1/q1/q2a/q2.txt",
            "site-packages/pkg1/q1/q2a/q3/q3a.txt",
            "site-packages/pkg1/q1/q2a/q3/q3b.txt",
            "site-packages/pkg1/q1/q2b/q2b.txt",
            "site-packages/pkg1/q1/q2c/c_mod.so",
            "site-packages/pkg1/q1/q2c/q2.txt",
            "site-packages/pkg1/q1/q2c/q3/q3a.txt",
            "site-packages/pkg1/q1/q2c/q3/q3b.txt",
        ],
    )
    analysis_test(
        name = name,
        impl = _test_optimized_grouping_complex_impl,
        target = name + "_files",
    )

_tests.append(_test_optimized_grouping_complex)

def _test_optimized_grouping_complex_impl(env, target):
    test_ctx = _ctx(workspace_name = env.ctx.workspace_name)
    entries = get_venv_symlinks(
        test_ctx,
        target.files.to_list(),
        package = "pkg1",
        version_str = "1.0",
        site_packages_root = env.ctx.label.package + "/site-packages",
    )
    actual = _venv_symlinks_from_entries(entries)

    rr = "{}/{}/site-packages/".format(test_ctx.workspace_name, env.ctx.label.package)
    expected = [
        _venv_symlink(
            "pkg1/a.txt",
            link_to_path = rr + "pkg1/a.txt",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/a.txt",
            ],
        ),
        _venv_symlink(
            "pkg1/b",
            link_to_path = rr + "pkg1/b",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/b/b_mod_so",
            ],
        ),
        _venv_symlink("pkg1/c", link_to_path = rr + "pkg1/c", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/c/c1.txt",
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/c/c2.txt",
        ]),
        _venv_symlink("pkg1/d", link_to_path = rr + "pkg1/d", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/d/d1.txt",
        ]),
        _venv_symlink("pkg1/dd", link_to_path = rr + "pkg1/dd", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/dd/dd1.txt",
        ]),
        _venv_symlink("pkg1/q1/q1.txt", link_to_path = rr + "pkg1/q1/q1.txt", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q1.txt",
        ]),
        _venv_symlink("pkg1/q1/q2a/libq.so", link_to_path = rr + "pkg1/q1/q2a/libq.so", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2a/libq.so",
        ]),
        _venv_symlink("pkg1/q1/q2a/q2.txt", link_to_path = rr + "pkg1/q1/q2a/q2.txt", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2a/q2.txt",
        ]),
        _venv_symlink("pkg1/q1/q2a/q3", link_to_path = rr + "pkg1/q1/q2a/q3", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2a/q3/q3a.txt",
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2a/q3/q3b.txt",
        ]),
        _venv_symlink("pkg1/q1/q2b", link_to_path = rr + "pkg1/q1/q2b", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2b/q2b.txt",
        ]),
        _venv_symlink("pkg1/q1/q2c/c_mod.so", link_to_path = rr + "pkg1/q1/q2c/c_mod.so", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2c/c_mod.so",
        ]),
        _venv_symlink("pkg1/q1/q2c/q2.txt", link_to_path = rr + "pkg1/q1/q2c/q2.txt", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2c/q2.txt",
        ]),
        _venv_symlink("pkg1/q1/q2c/q3", link_to_path = rr + "pkg1/q1/q2c/q3", files = [
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2c/q3/q3a.txt",
            "tests/venv_site_packages_libs/app_files_building/site-packages/pkg1/q1/q2c/q3/q3b.txt",
        ]),
    ]
    expected = sorted(expected, key = lambda e: (e.link_to_path, e.venv_path))
    env.expect.that_collection(
        actual,
    ).contains_exactly(expected)
    _, conflicts = build_link_map(test_ctx, entries, return_conflicts = True)

    # The point of the optimization is to avoid having to merge conflicts.
    env.expect.that_collection(conflicts).contains_exactly([])

def _test_optimized_grouping_single_toplevel(name):
    empty_files(
        name = name + "_files",
        paths = [
            "site-packages/pkg2/__init__.py",
            "site-packages/pkg2/a.txt",
            "site-packages/pkg2/b_mod_so",
        ],
    )
    analysis_test(
        name = name,
        impl = _test_optimized_grouping_single_toplevel_impl,
        target = name + "_files",
    )

_tests.append(_test_optimized_grouping_single_toplevel)

def _test_optimized_grouping_single_toplevel_impl(env, target):
    test_ctx = _ctx(workspace_name = env.ctx.workspace_name)
    entries = get_venv_symlinks(
        test_ctx,
        target.files.to_list(),
        package = "pkg2",
        version_str = "1.0",
        site_packages_root = env.ctx.label.package + "/site-packages",
    )
    actual = _venv_symlinks_from_entries(entries)

    rr = "{}/{}/site-packages/".format(test_ctx.workspace_name, env.ctx.label.package)
    expected = [
        _venv_symlink(
            "pkg2",
            link_to_path = rr + "pkg2",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/pkg2/__init__.py",
                "tests/venv_site_packages_libs/app_files_building/site-packages/pkg2/a.txt",
                "tests/venv_site_packages_libs/app_files_building/site-packages/pkg2/b_mod_so",
            ],
        ),
    ]
    expected = sorted(expected, key = lambda e: (e.link_to_path, e.venv_path))

    env.expect.that_collection(
        actual,
    ).contains_exactly(expected)

    _, conflicts = build_link_map(test_ctx, entries, return_conflicts = True)

    # The point of the optimization is to avoid having to merge conflicts.
    env.expect.that_collection(conflicts).contains_exactly([])

def _test_optimized_grouping_implicit_namespace_packages(name):
    empty_files(
        name = name + "_files",
        paths = [
            # NOTE: An alphanumeric name with underscores is used to verify
            # name matching is correct.
            "site-packages/name_space9/part1/foo.py",
            "site-packages/name_space9/part2/bar.py",
            "site-packages/name_space9-1.0.dist-info/METADATA",
        ],
    )
    analysis_test(
        name = name,
        impl = _test_optimized_grouping_implicit_namespace_packages_impl,
        target = name + "_files",
    )

_tests.append(_test_optimized_grouping_implicit_namespace_packages)

def _test_optimized_grouping_implicit_namespace_packages_impl(env, target):
    test_ctx = _ctx(workspace_name = env.ctx.workspace_name)
    entries = get_venv_symlinks(
        test_ctx,
        target.files.to_list(),
        package = "pkg3",
        version_str = "1.0",
        site_packages_root = env.ctx.label.package + "/site-packages",
    )
    actual = _venv_symlinks_from_entries(entries)

    rr = "{}/{}/site-packages/".format(test_ctx.workspace_name, env.ctx.label.package)
    expected = [
        _venv_symlink(
            "name_space9/part1",
            link_to_path = rr + "name_space9/part1",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/name_space9/part1/foo.py",
            ],
        ),
        _venv_symlink(
            "name_space9/part2",
            link_to_path = rr + "name_space9/part2",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/name_space9/part2/bar.py",
            ],
        ),
        _venv_symlink(
            "name_space9-1.0.dist-info",
            link_to_path = rr + "name_space9-1.0.dist-info",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/name_space9-1.0.dist-info/METADATA",
            ],
        ),
    ]
    expected = sorted(expected, key = lambda e: (e.link_to_path, e.venv_path))
    env.expect.that_collection(
        actual,
    ).contains_exactly(expected)

    _, conflicts = build_link_map(test_ctx, entries, return_conflicts = True)

    # The point of the optimization is to avoid having to merge conflicts.
    env.expect.that_collection(conflicts).contains_exactly([])

def _test_optimized_grouping_pkgutil_namespace_packages(name):
    empty_files(
        name = name + "_files",
        paths = [
            "site-packages/pkgutilns/__init__.py",
            "site-packages/pkgutilns/foo.py",
            # Special cases: These dirnames under site-packages are always
            # treated as namespace packages
            "site-packages/nvidia/whatever/w.py",
        ],
    )
    analysis_test(
        name = name,
        impl = _test_optimized_grouping_pkgutil_namespace_packages_impl,
        target = name + "_files",
    )

_tests.append(_test_optimized_grouping_pkgutil_namespace_packages)

def _test_optimized_grouping_pkgutil_namespace_packages_impl(env, target):
    test_ctx = _ctx(workspace_name = env.ctx.workspace_name)
    files = target.files.to_list()
    ns_inits = [f for f in files if f.basename == "__init__.py"]

    entries = get_venv_symlinks(
        test_ctx,
        files,
        package = "pkgutilns",
        version_str = "1.0",
        site_packages_root = env.ctx.label.package + "/site-packages",
        namespace_package_files = ns_inits,
    )
    actual = _venv_symlinks_from_entries(entries)

    rr = "{}/{}/site-packages/".format(test_ctx.workspace_name, env.ctx.label.package)
    expected = [
        _venv_symlink(
            "pkgutilns/__init__.py",
            link_to_path = rr + "pkgutilns/__init__.py",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/pkgutilns/__init__.py",
            ],
        ),
        _venv_symlink(
            "pkgutilns/foo.py",
            link_to_path = rr + "pkgutilns/foo.py",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/pkgutilns/foo.py",
            ],
        ),
        _venv_symlink(
            "nvidia/whatever",
            link_to_path = rr + "nvidia/whatever",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/nvidia/whatever/w.py",
            ],
        ),
    ]
    expected = sorted(expected, key = lambda e: (e.link_to_path, e.venv_path))
    env.expect.that_collection(
        actual,
    ).contains_exactly(expected)

    _, conflicts = build_link_map(test_ctx, entries, return_conflicts = True)

    # The point of the optimization is to avoid having to merge conflicts.
    env.expect.that_collection(conflicts).contains_exactly([])

def _test_optimized_grouping_pkgutil_whls(name):
    """Verify that the whl_library pkgutli style detection logic works."""
    py_library(
        name = name + "_lib",
        deps = [
            "@pkgutil_nspkg1//:pkg",
            "@pkgutil_nspkg2//:pkg",
        ],
        target_compatible_with = SUPPORTS_BZLMOD_UNIXY,
    )
    analysis_test(
        name = name,
        impl = _test_optimized_grouping_pkgutil_whls_impl,
        target = name + "_lib",
        config_settings = {
            labels.VENVS_SITE_PACKAGES: "yes",
        },
        attr_values = dict(
            target_compatible_with = SUPPORTS_BZLMOD_UNIXY,
        ),
    )

_tests.append(_test_optimized_grouping_pkgutil_whls)

def _test_optimized_grouping_pkgutil_whls_impl(env, target):
    test_ctx = _ctx(workspace_name = env.ctx.workspace_name)
    actual_raw_entries = target[PyInfo].venv_symlinks.to_list()

    actual = _venv_symlinks_from_entries(actual_raw_entries)

    # The important condition is that the top-level 'nspkg' directory
    # is NOT linked because it's a pkgutil namespace package.
    env.expect.that_collection(actual).contains_exactly([
        # Entries from pkgutil_ns1
        _venv_symlink(
            "nspkg/__init__.py",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg1/site-packages/nspkg/__init__.py",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg1/site-packages/nspkg/__init__.py",
            ],
        ),
        _venv_symlink(
            "nspkg/one",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg1/site-packages/nspkg/one",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg1/site-packages/nspkg/one/a.txt",
            ],
        ),
        _venv_symlink(
            "pkgutil_nspkg1-1.0.dist-info",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg1/site-packages/pkgutil_nspkg1-1.0.dist-info",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg1/site-packages/pkgutil_nspkg1-1.0.dist-info/INSTALLER",
                "../+internal_dev_deps+pkgutil_nspkg1/site-packages/pkgutil_nspkg1-1.0.dist-info/METADATA",
                "../+internal_dev_deps+pkgutil_nspkg1/site-packages/pkgutil_nspkg1-1.0.dist-info/WHEEL",
            ],
        ),
        # Entries from pkgutil_ns2
        _venv_symlink(
            "nspkg/__init__.py",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg2/site-packages/nspkg/__init__.py",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg2/site-packages/nspkg/__init__.py",
            ],
        ),
        _venv_symlink(
            "nspkg/two",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg2/site-packages/nspkg/two",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg2/site-packages/nspkg/two/b.txt",
            ],
        ),
        _venv_symlink(
            "pkgutil_nspkg2-1.0.dist-info",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg2/site-packages/pkgutil_nspkg2-1.0.dist-info",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg2/site-packages/pkgutil_nspkg2-1.0.dist-info/INSTALLER",
                "../+internal_dev_deps+pkgutil_nspkg2/site-packages/pkgutil_nspkg2-1.0.dist-info/METADATA",
                "../+internal_dev_deps+pkgutil_nspkg2/site-packages/pkgutil_nspkg2-1.0.dist-info/WHEEL",
            ],
        ),
    ])

    # Verifying that the expected VenvSymlink structure is processed with minimal number
    # of conflicts (Just the single pkgutil style __init__.py file)
    _, conflicts = build_link_map(test_ctx, actual_raw_entries, return_conflicts = True)
    env.expect.that_collection(_venv_symlinks_from_entries(conflicts[0])).contains_exactly([
        _venv_symlink(
            "nspkg/__init__.py",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg1/site-packages/nspkg/__init__.py",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg1/site-packages/nspkg/__init__.py",
            ],
        ),
        _venv_symlink(
            "nspkg/__init__.py",
            link_to_path = "+internal_dev_deps+pkgutil_nspkg2/site-packages/nspkg/__init__.py",
            files = [
                "../+internal_dev_deps+pkgutil_nspkg2/site-packages/nspkg/__init__.py",
            ],
        ),
    ])
    env.expect.that_collection(conflicts).has_size(1)

def _test_package_version_filtering(name):
    analysis_test(
        name = name,
        impl = _test_package_version_filtering_impl,
        target = "//python:none",
    )

_tests.append(_test_package_version_filtering)

def _test_package_version_filtering_impl(env, _):
    entries = [
        _entry("foo", "+pypi_v1/site-packages/foo", [
            _file("../+pypi_v1/site-packages/foo/foo.txt"),
        ], package = "foo", version = "1.0"),
        _entry("foo", "+pypi_v2/site-packages/foo", [
            _file("../+pypi_v2/site-packages/foo/bar.txt"),
        ], package = "foo", version = "2.0"),
    ]

    actual = build_link_map(_ctx(), entries)

    expected_libs = {
        "foo": "+pypi_v1/site-packages/foo",
    }
    env.expect.that_dict(actual[VenvSymlinkKind.LIB]).contains_exactly(expected_libs)

def _test_malformed_entry(name):
    analysis_test(
        name = name,
        impl = _test_malformed_entry_impl,
        target = "//python:none",
    )

_tests.append(_test_malformed_entry)

def _test_malformed_entry_impl(env, _):
    entries = [
        _entry(
            "a",
            "+pypi_a/site-packages/a",
            # This file is outside the link_to_path, so it should be ignored.
            [_file("../+pypi_a/site-packages/outside.txt")],
        ),
        # A second, conflicting, entry is added to force merging of the known
        # files. Without this, there's no conflict, so files is never
        # considered.
        _entry(
            "a",
            "+pypi_b/site-packages/a",
            [_file("../+pypi_b/site-packages/outside.txt")],
        ),
    ]

    actual = build_link_map(_ctx(), entries)
    env.expect.that_dict(actual).contains_exactly({
        VenvSymlinkKind.LIB: {},
    })

def _test_complex_namespace_packages(name):
    analysis_test(
        name = name,
        impl = _test_complex_namespace_packages_impl,
        target = "//python:none",
    )

_tests.append(_test_complex_namespace_packages)

def _test_complex_namespace_packages_impl(env, _):
    entries = [
        _entry("a/b", "+pypi_a_b/site-packages/a/b", [
            _file("../+pypi_a_b/site-packages/a/b/b.txt"),
        ]),
        _entry("a/c", "+pypi_a_c/site-packages/a/c", [
            _file("../+pypi_a_c/site-packages/a/cc.txt"),
        ]),
        _entry("x/y/z", "+pypi_x_y_z/site-packages/x/y/z", [
            _file("../+pypi_x_y_z/site-packages/x/y/z/z.txt"),
        ]),
        _entry("foo", "+pypi_foo/site-packages/foo", [
            _file("../+pypi_foo/site-packages/foo/foo.txt"),
        ]),
        _entry("foobar", "+pypi_foobar/site-packages/foobar", [
            _file("../+pypi_foobar/site-packages/foobar/foobar.txt"),
        ]),
    ]

    actual = build_link_map(_ctx(), entries)
    expected_libs = {
        "a/b": "+pypi_a_b/site-packages/a/b",
        "a/c": "+pypi_a_c/site-packages/a/c",
        "foo": "+pypi_foo/site-packages/foo",
        "foobar": "+pypi_foobar/site-packages/foobar",
        "x/y/z": "+pypi_x_y_z/site-packages/x/y/z",
    }
    env.expect.that_dict(actual[VenvSymlinkKind.LIB]).contains_exactly(expected_libs)

def _test_empty_and_trivial_inputs(name):
    analysis_test(
        name = name,
        impl = _test_empty_and_trivial_inputs_impl,
        target = "//python:none",
    )

_tests.append(_test_empty_and_trivial_inputs)

def _test_empty_and_trivial_inputs_impl(env, _):
    # Test with empty list of entries
    actual = build_link_map(_ctx(), [])
    env.expect.that_dict(actual).contains_exactly({})

    # Test with an entry with no files
    entries = [_entry("a", "+pypi_a/site-packages/a", [])]
    actual = build_link_map(_ctx(), entries)
    env.expect.that_dict(actual).contains_exactly({
        VenvSymlinkKind.LIB: {"a": "+pypi_a/site-packages/a"},
    })

def _test_multiple_venv_symlink_kinds(name):
    analysis_test(
        name = name,
        impl = _test_multiple_venv_symlink_kinds_impl,
        target = "//python:none",
    )

_tests.append(_test_multiple_venv_symlink_kinds)

def _test_multiple_venv_symlink_kinds_impl(env, _):
    entries = [
        _entry(
            "libfile",
            "+pypi_lib/site-packages/libfile",
            [_file("../+pypi_lib/site-packages/libfile/lib.txt")],
            kind = VenvSymlinkKind.LIB,
        ),
        _entry(
            "binfile",
            "+pypi_bin/bin/binfile",
            [_file("../+pypi_bin/bin/binfile/bin.txt")],
            kind = VenvSymlinkKind.BIN,
        ),
        _entry(
            "includefile",
            "+pypi_include/include/includefile",
            [_file("../+pypi_include/include/includefile/include.h")],
            kind =
                VenvSymlinkKind.INCLUDE,
        ),
    ]

    actual = build_link_map(_ctx(), entries)

    expected_libs = {
        "libfile": "+pypi_lib/site-packages/libfile",
    }
    env.expect.that_dict(actual[VenvSymlinkKind.LIB]).contains_exactly(expected_libs)

    expected_bins = {
        "binfile": "+pypi_bin/bin/binfile",
    }
    env.expect.that_dict(actual[VenvSymlinkKind.BIN]).contains_exactly(expected_bins)

    expected_includes = {
        "includefile": "+pypi_include/include/includefile",
    }
    env.expect.that_dict(actual[VenvSymlinkKind.INCLUDE]).contains_exactly(expected_includes)

    env.expect.that_dict(actual).keys().contains_exactly([
        VenvSymlinkKind.LIB,
        VenvSymlinkKind.BIN,
        VenvSymlinkKind.INCLUDE,
    ])

def _test_shared_library_symlinking(name):
    empty_files(
        name = name + "_files",
        # NOTE: Test relies upon order
        paths = [
            "site-packages/bar/libs/liby.so",
            "site-packages/bar/x.py",
            "site-packages/bar/y.so",
            "site-packages/foo.libs/libx.so",
            "site-packages/foo/a.py",
            "site-packages/foo/b.so",
            "site-packages/root.pth",
            "site-packages/root.py",
            "site-packages/root.so",
        ],
    )
    analysis_test(
        name = name,
        impl = _test_shared_library_symlinking_impl,
        target = name + "_files",
    )

_tests.append(_test_shared_library_symlinking)

def _test_shared_library_symlinking_impl(env, target):
    srcs = target.files.to_list()
    actual_entries = get_venv_symlinks(
        _ctx(),
        srcs,
        package = "foo",
        version_str = "1.0",
        site_packages_root = env.ctx.label.package + "/site-packages",
    )

    actual = _venv_symlinks_from_entries(actual_entries)

    env.expect.that_collection(actual).contains_at_least([
        _venv_symlink(
            "bar/libs/liby.so",
            link_to_path = "_main/tests/venv_site_packages_libs/app_files_building/site-packages/bar/libs/liby.so",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/bar/libs/liby.so",
            ],
        ),
        _venv_symlink(
            "foo.libs/libx.so",
            link_to_path = "_main/tests/venv_site_packages_libs/app_files_building/site-packages/foo.libs/libx.so",
            files = [
                "tests/venv_site_packages_libs/app_files_building/site-packages/foo.libs/libx.so",
            ],
        ),
    ])

    actual = build_link_map(_ctx(), actual_entries)

    # The important condition is that each lib*.so file is linked directly.
    expected_libs = {
        "bar/libs/liby.so": srcs[0],
        "foo.libs/libx.so": srcs[3],
    }
    env.expect.that_dict(actual[VenvSymlinkKind.LIB]).contains_at_least(expected_libs)

def app_files_building_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
