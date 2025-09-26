
def updater(
        name,
        dependencies,
        version_file,
        jq_toolchain = "@jq_toolchains//:resolved_toolchain",
        update_script = "@envoy_toolshed//dependency:bazel-update.sh",
        post_script = None,
        data = None,
        deps = None,
        dep_search = None,
        sha_search = None,
        version_search = None,
        repo_selector = None,
        sha_selector = None,
        url_selector = None,
        version_path_replace = None,
        version_selector = None,
        toolchains = None,
        pydict = False,
        **kwargs,
):
    toolchains = [jq_toolchain] + (toolchains or [])
    deps = deps or []
    data = (data or []) + [
        jq_toolchain,
        update_script,
        dependencies,
        version_file,
    ]
    args = [
        "$(location %s)" % version_file,
        "$(location %s)" % dependencies,
    ]
    env = {
        "JQ_BIN": "$(JQ_BIN)",
    }
    if pydict:
        env["DEP_SEARCH"] = "__DEP__ = dict("
        env["SHA_SEARCH"] = "sha256 = \"__EXISTING_SHA__\","
        env["VERSION_SEARCH"] = "version = \"__EXISTING_VERSION__\","

    if dep_search:
        env["DEP_SEARCH"] = dep_search
    if sha_search:
        env["SHA_SEARCH"] = sha_search
    if version_search:
        env["VERSION_SEARCH"] = version_search
    if repo_selector:
        env["REPO_SELECTOR"] = repo_selector
    if sha_selector:
        env["SHA_SELECTOR"] = sha_selector
    if url_selector:
        env["URL_SELECTOR"] = url_selector
    if version_path_replace:
        env["VERSION_PATH_REPLACE"] = version_path_replace
    if version_selector:
        env["VERSION_SELECTOR"] = version_selector

    if post_script:
        data += [post_script]
        env["VERSION_UPDATE_POST_SCRIPT"] = "$(location %s)" % post_script

    native.sh_binary(
        name = name,
        srcs = [update_script],
        data = data,
        env = env,
        args = args,
        deps = deps,
        toolchains = toolchains,
        **kwargs,
    )
