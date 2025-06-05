load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("//private/lib:urls.bzl", "get_m2local_url")

def escape(string):
    for char in [".", "-", ":", "/", "+"]:
        string = string.replace(char, "_")
    return string.replace("[", "").replace("]", "").split(",")[0]

def download_pinned_deps(mctx, artifacts, http_files, has_m2local):
    seen_repo_names = []

    for artifact in artifacts:
        http_file_repository_name = escape(artifact["coordinates"])

        if http_file_repository_name in http_files:
            continue

        urls = []
        artifact_urls = artifact["urls"]

        # Since the url returned from here will be included in the bazel lock file, only include local urls if there are
        # no remote urls. This is less than ideal, but is the only way to prevent the lock file clashing between users.
        if len(artifact_urls) and [None] != artifact_urls:
            urls.extend(artifact_urls)
        elif has_m2local:
            m2local_url = get_m2local_url(mctx.os, mctx.path, artifact)
            if m2local_url:
                urls.append(m2local_url)

        # If we can't download the artifact from anywhere, run away
        if len(urls) == 0:
            continue

        seen_repo_names.append(http_file_repository_name)
        http_files.append(http_file_repository_name)

        http_file(
            name = http_file_repository_name,
            sha256 = artifact["sha256"],
            urls = urls,
            # https://github.com/bazelbuild/rules_jvm_external/issues/1028
            downloaded_file_path = "v1/%s" % artifact["file"] if artifact["file"] else artifact["file"],
        )

    return seen_repo_names
