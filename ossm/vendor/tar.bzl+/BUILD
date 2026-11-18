load("@bazel_lib//lib:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//tar:mtree.bzl", "mtree_spec")

# Test case for mtree_spec: Ensure that multiple entries at the root directory are handled correctly (bug #851)
# See lib/tests/tar/BUILD.bazel for why this is here.
write_file(
    name = "tar_test13_main",
    out = "13project/__main__.py",
    content = ["__main__.py"],
)

write_file(
    name = "tar_test13_bin",
    out = "13project_bin",
    content = ["project_bin"],
)

mtree_spec(
    name = "tar_test13_mtree_unsorted",
    srcs = [
        ":tar_test13_bin",
        ":tar_test13_main",
    ],
)

# NOTE: On some systems, the mtree_spec output can have a different order.
#       To make the test less brittle, we sort the mtree output and replace the BINDIR with a constant placeholder
genrule(
    name = "tar_test13_mtree",
    srcs = [":tar_test13_mtree_unsorted"],
    outs = ["actual13.mtree"],
    cmd = "sort $< | sed 's#$(BINDIR)#{BINDIR}#' >$@",
)

diff_test(
    name = "tar_test13",
    file1 = "tar_test13_mtree",
    file2 = "//tar/tests:expected13.mtree",
)
