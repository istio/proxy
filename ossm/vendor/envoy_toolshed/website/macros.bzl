load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup", "pkg_files")
load("@rules_pkg//pkg:pkg.bzl", "pkg_tar")

def static_website(
        name,
        content = ":content",
        theme = ":theme",
        config = ":config",
        content_path = "content",
        data = ":data",
        deps = None,
        compressor = None,
        compressor_args = None,
        exclude = [
            "archives.html",
            "authors.html",
            "categories.html",
            "external",
            "tags.html",
            "pages",
            "theme/.webassets-cache",
            "theme/css/_sass",
            "theme/css/main.scss"
        ],
        generator = "@envoy_toolshed//website/tools/pelican",
        extension = "tar.gz",
        mappings = {
            "theme/css": "theme/static/css",
            "theme/js": "theme/static/js",
            "theme/images": "theme/static/images",
            "theme/templates/extra": "theme/templates",
        },
        output_path = "output",
        srcs = None,
        url = "",
        visibility = ["//visibility:public"],
):
    name_html = "%s_html" % name
    name_sources = "%s_sources" % name
    name_website = "%s_website" % name
    name_website_tarball = "%s_website.tar.gz" % (name_website)

    sources = [
        config,
        content,
        theme,
    ]

    if data:
        sources += [data]

    pkg_tar(
        name = name_sources,
        compressor = compressor,
        compressor_args = compressor_args,
        extension = extension,
        srcs = sources,
    )

    tools = [generator]

    extra_srcs = [
        name_sources,
    ] + sources

    if url:
        extra_srcs.append(url)
        url = "export SITEURL=$$(cat $(location %s))" % url

    decompressor_args = ""
    if compressor:
        decompressor_args = "--use-compress-program=$(location %s)" % compressor
        tools += [compressor]

    exclude_args = " ".join(["--exclude=%s" % item for item in exclude])
    mapping_commands = "\n".join([
        "mkdir -p %s \ncp -a %s/* %s" % (dest, src, dest)
        for src, dest in mappings.items()
    ])

    native.genrule(
        name = name_website,
        cmd = """
        SOURCE="$(location %s)"
        DECOMPRESS_ARGS="%s"
        GENERATOR="$(location %s)"
        CONTENT="%s"
        OUTPUT="%s"
        MAPPING="%s"
        EXCLUDES="%s"
        %s

        tar "$${DECOMPRESS_ARGS}" -xf $$SOURCE

        while IFS= read -r CMD; do
            $$CMD
        done <<< "$$MAPPING"

        $$GENERATOR "$$CONTENT"

        tar cfh $@ $$EXCLUDES -C "$$OUTPUT" .
        """ % (name_sources, decompressor_args, generator, content_path, output_path, mapping_commands, exclude_args, url),
        outs = [name_website_tarball],
        srcs = extra_srcs,
        tools = tools,
    )

    pkg_tar(
        name = name_html,
        deps = [name_website] + (deps or []),
        srcs = srcs or [],
        compressor = compressor,
        compressor_args = compressor_args,
        extension = extension,
        visibility = visibility,
    )

    native.alias(
        name = name,
        actual = name_html,
        visibility = visibility,
    )

def website_theme(
        name,
        css = "@envoy_toolshed//website/theme/css",
        css_extra = None,
        home = "@envoy_toolshed//website/theme:home",
        images = "@envoy_toolshed//website/theme/images",
        images_extra = None,
        js = None,
        templates = "@envoy_toolshed//website/theme/templates",
        templates_extra = None,
        visibility = ["//visibility:public"],
):

    name_home = "home_%s" % name
    sources = [
        css,
        templates,
    ]
    if templates_extra:
        sources += [templates_extra]
    if css_extra:
        sources += [css_extra]
    if js:
        sources += [js]
    if images:
        sources += [images]
        if images_extra:
            sources += [images_extra]

    pkg_files(
        name = name_home,
        srcs = [home],
        strip_prefix = "",
        prefix = "theme/templates",
    )

    sources += [":%s" % name_home]

    pkg_filegroup(
        name = name,
        srcs = sources,
        visibility = visibility,
    )
