"""Implementation for print_rel_notes."""

# buildifier: disable=function-docstring
def print_rel_notes(
        name,
        repo,
        version,
        artifact_name = None,
        outs = None,
        setup_file = "",
        deps_method = "",
        toolchains_method = "",
        org = "bazelbuild",
        changelog = None,
        mirror_host = None):
    if not artifact_name:
        artifact_name = ":%s-%s.tar.gz" % (repo, version)

    # Must use Label to get a path relative to the rules_pkg repository,
    # instead of the calling BUILD file.
    print_rel_notes_helper = Label("//pkg/releasing:print_rel_notes")
    tools = [print_rel_notes_helper]
    cmd = [
        "LC_ALL=C.UTF-8 $(location %s)" % str(print_rel_notes_helper),
        "--org=%s" % org,
        "--repo=%s" % repo,
        "--version=%s" % version,
        "--tarball=$(location %s)" % artifact_name,
    ]
    if setup_file:
        cmd.append("--setup_file=%s" % setup_file)
    if deps_method:
        cmd.append("--deps_method=%s" % deps_method)
    if toolchains_method:
        cmd.append("--toolchains_method=%s" % toolchains_method)
    if changelog:
        cmd.append("--changelog=$(location %s)" % changelog)

        # We should depend on a changelog as a tool so that it is always built
        # for the host configuration. If the changelog is generated on the fly,
        # then we would have to run commands against our revision control
        # system. That only makes sense locally on the host, because the
        # revision history is never exported to a remote build system.
        tools.append(changelog)
    if mirror_host:
        cmd.append("--mirror_host=%s" % mirror_host)
    cmd.append(">$@")
    native.genrule(
        name = name,
        srcs = [
            artifact_name,
        ],
        outs = outs or [name + ".txt"],
        cmd = " ".join(cmd),
        tools = tools,
    )
