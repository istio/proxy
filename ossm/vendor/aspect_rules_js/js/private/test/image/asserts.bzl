"Make shorter assertions"

load("@aspect_bazel_lib//lib:write_source_files.bzl", "write_source_file")
load("//js:defs.bzl", "js_image_layer")

def make_js_image_layer(name, **kwargs):
    js_image_layer(
        name = name,
        testonly = 1,
        tags = [
            # mode bit on files aren't stable between RBE and Local since RBE isn't aware of git which tracks permissions for files.
            # we don't care about unstable inputs because it's not our responsibility to keep them stable which would expand api surface for js_image_layer
            "no-remote-exec",
        ],
        **kwargs
    )

    native.filegroup(
        name = name + "_app_layer",
        srcs = [name],
        output_group = "app",
        testonly = 1,
    )

    native.filegroup(
        name = name + "_node_modules_layer",
        srcs = [name],
        output_group = "node_modules",
        testonly = 1,
    )

# buildifier: disable=function-docstring
def assert_tar_listing(name, actual, expected):
    actual_listing = "_{}_listing".format(name)
    native.genrule(
        name = actual_listing,
        srcs = [actual],
        testonly = True,
        outs = ["_{}.listing".format(name)],
        # Install gnu-tar on macos to match CI
        # And replace tar below with  /opt/homebrew/opt/gnu-tar/libexec/gnubin/tar
        cmd = 'TZ="UTC" LC_ALL="en_US.UTF-8" tar -tvf $(locations {}) >$@'.format(actual),
    )

    write_source_file(
        name = name,
        in_file = actual_listing,
        out_file = expected,
        testonly = True,
        # TODO: js_image_layer is broken with bzlmod https://github.com/aspect-build/rules_js/issues/1530
        target_compatible_with = select({
            "@aspect_bazel_lib//lib:bzlmod": ["@platforms//:incompatible"],
            "//conditions:default": [],
        }),
    )
