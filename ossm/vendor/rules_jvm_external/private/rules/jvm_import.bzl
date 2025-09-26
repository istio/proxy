# Stripped down version of a java_import Starlark rule, without invoking ijar
# to create interface jars.

# Inspired by Square's implementation of `raw_jvm_import` [0] and discussions
# on the GitHub thread [1] about ijar's interaction with Kotlin JARs.
#
# [0]: https://github.com/square/bazel_maven_repository/pull/48
# [1]: https://github.com/bazelbuild/bazel/issues/4549

load("@rules_java//java:defs.bzl", "JavaInfo")
load("@rules_license//rules:providers.bzl", "PackageInfo")
load("//private/lib:coordinates.bzl", "to_external_form", "to_purl", "unpack_coordinates")
load("//private/lib:urls.bzl", "scheme_and_host")
load("//settings:stamp_manifest.bzl", "StampManifestProvider")

def _jvm_import_impl(ctx):
    if not ctx.attr.jar and not ctx.attr.jars:
        fail("The `jar` attribute is mandatory.")

    if ctx.attr.jar and ctx.attr.jars:
        fail("Only specify the `jar` attribute.")

    if len(ctx.files.jars) > 1:
        fail("Please only specify one jar to import in the jars attribute.")

    if not ctx.attr.jar:
        print(ctx.attr.name, "The `jars` attribute is deprecated and will be removed in `rules_jvm_external` 7.0. Please use the `jar` attribute instead.")

    injar = ctx.file.jar if ctx.attr.jar else ctx.files.jars[0]

    if ctx.attr._stamp_manifest[StampManifestProvider].stamp_enabled:
        outjar = ctx.actions.declare_file("processed_" + injar.basename, sibling = injar)
        args = ctx.actions.args()
        args.add_all(["--source", injar, "--output", outjar])
        args.add("--manifest-entry", ctx.label, format = "Target-Label:%s")
        ctx.actions.run(
            executable = ctx.executable._add_jar_manifest_entry,
            arguments = [args],
            inputs = [injar],
            outputs = [outjar],
            mnemonic = "StampJarManifest",
            progress_message = "Stamping the manifest of %{label}",
        )
    else:
        outjar = injar

    compilejar = ctx.actions.declare_file("header_" + injar.basename, sibling = injar)
    args = ctx.actions.args()
    args.add_all(["--source", outjar, "--output", compilejar])

    # We need to remove the `Class-Path` entry since bazel 4.0.0 forces `javac`
    # to run `-Xlint:path` no matter what other flags are passed. Bazel
    # manages the classpath for us, so the `Class-Path` manifest entry isn't
    # needed. Worse, if it's there and the jars listed in it aren't found,
    # the lint check will emit a `bad path element` warning. We get quiet and
    # correct builds if we remove the `Class-Path` manifest entry entirely.
    args.add_all(["--remove-entry", "Class-Path"])

    # Make sure the compile jar is safe to compile with
    args.add("--make-safe")

    ctx.actions.run(
        executable = ctx.executable._add_jar_manifest_entry,
        arguments = [args],
        inputs = [outjar],
        outputs = [compilejar],
        mnemonic = "CreateCompileJar",
        progress_message = "Creating compile jar for %s" % ctx.label,
    )

    additional_providers = []
    if ctx.attr.maven_coordinates:
        unpacked = unpack_coordinates(ctx.attr.maven_coordinates)

        additional_providers.append(
            PackageInfo(
                type = "jvm_import",
                label = ctx.label,
                package_url = ctx.attr.maven_url,
                package_version = unpacked.version,
                package_name = to_external_form(ctx.attr.maven_coordinates),
                purl = to_purl(ctx.attr.maven_coordinates, scheme_and_host(ctx.attr.maven_url)),
            ),
        )

    return [
        DefaultInfo(
            files = depset([outjar]),
        ),
        JavaInfo(
            compile_jar = compilejar,
            output_jar = outjar,
            source_jar = ctx.file.srcjar,
            deps = [
                dep[JavaInfo]
                for dep in ctx.attr.deps
                if JavaInfo in dep
            ],
            neverlink = ctx.attr.neverlink,
        ),
    ] + additional_providers

jvm_import = rule(
    attrs = {
        "jars": attr.label_list(
            doc = "Deprecated. Scheduled for removal in `@rules_jvm_external` 7.0.",
            allow_files = True,
            cfg = "target",
        ),
        "jar": attr.label(
            allow_single_file = True,
            cfg = "target",
        ),
        "srcjar": attr.label(
            allow_single_file = True,
            cfg = "target",
        ),
        "deps": attr.label_list(
            default = [],
            providers = [JavaInfo],
        ),
        "neverlink": attr.bool(
            default = False,
        ),
        "maven_coordinates": attr.string(
            doc = "The maven coordinates that the `jar` can be downloaded from.",
        ),
        "maven_url": attr.string(
            doc = "URL from where `jar` will be downloaded from.",
        ),
        "_add_jar_manifest_entry": attr.label(
            executable = True,
            cfg = "exec",
            default = "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:AddJarManifestEntry",
        ),
        "_stamp_manifest": attr.label(
            default = "@rules_jvm_external//settings:stamp_manifest",
        ),
    },
    implementation = _jvm_import_impl,
    provides = [JavaInfo],
)
