"Test module extension to create a test repository"

def _test_repo_impl(rctx):
    rctx.file("BUILD.bazel", """\
exports_files(["foobar.txt"], visibility = ["//visibility:public"])
""", executable = False)

    rctx.file("foobar.txt", "foobar\n")

test_repo = repository_rule(
    local = True,
    implementation = _test_repo_impl,
)

def _test_ext_impl(_):
    test_repo(name = "test")

test_ext = module_extension(
    implementation = _test_ext_impl,
)
