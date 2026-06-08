"""Override diff_test behaviour to ignore carriage returns in order to
test jq output on Windows. See https://github.com/stedolan/jq/issues/92.
"""

load("//jq:diff_test.bzl", _diff_test = "diff_test")

def diff_test(name, file1, file2):
    """Perform a diff_test ignoring carriage returns

    Args:
        name: name of the test rule
        file1: first file to compare
        file2: second file to compare
    """
    test_files = []
    for i, file in enumerate([file1, file2], start = 1):
        if file[0] == ":":
            target = file[1:]
        else:
            target = file

        stripped_file = "%s_file%d_stripped" % (name, i)

        native.genrule(
            name = "%s_file%d" % (name, i),
            srcs = [file],
            outs = [stripped_file],
            cmd = "cat $(execpath :{target}) | sed \"s#\\r##\" > $@".format(target = target),
        )
        test_files.append(stripped_file)

    _diff_test(
        name = name,
        file1 = test_files[0],
        file2 = test_files[1],
    )
