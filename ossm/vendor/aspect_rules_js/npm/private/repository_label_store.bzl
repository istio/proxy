"""In order to avoid late restarts, repository rules should pre-compute dynamic labels and
paths to static & dynamic labels. This helper "class" abstracts that into a tidy interface.

See https://github.com/bazelbuild/bazel-gazelle/issues/1175 &&
https://github.com/bazelbuild/rules_nodejs/issues/2620 for more context. A fix in Bazel may
resolve the underlying issue in the future https://github.com/bazelbuild/bazel/issues/16162.
"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@aspect_bazel_lib//lib:utils.bzl", "is_bazel_6_or_greater")

################################################################################
def _make_sibling_label(sibling_label, path):
    if path.startswith("./"):
        path = path[2:]
    dirname = paths.dirname(sibling_label.name)
    if path.startswith("../"):
        # we have no idea what package this sibling is in so just assume the root package which works for repository rules
        return Label("{}{}//:{}".format("@@" if is_bazel_6_or_greater() else "@", sibling_label.workspace_name, paths.normalize(paths.join(sibling_label.package, dirname, path))))
    else:
        return Label("{}{}//{}:{}".format("@@" if is_bazel_6_or_greater() else "@", sibling_label.workspace_name, sibling_label.package, paths.join(dirname, path)))

################################################################################
def _seed_root(priv, rctx_path, label):
    if priv["root"] and label.workspace_name != priv["root"]["workspace"]:
        fail("cannot seed_root twice with different workspaces")
    if priv["root"]:
        # already seed_rooted with the same workspace
        return
    seed_root_path = str(rctx_path(label))
    seed_root_depth = len(paths.join(label.package, label.name).split("/"))
    priv["root"] = {
        "workspace": label.workspace_name,
        "path": "/".join(seed_root_path.split("/")[:-seed_root_depth]),
    }

################################################################################
def _add(priv, rctx_path, repo_root, key, label, seed_root = False):
    priv["labels"][key] = label
    priv["paths"][key] = str(rctx_path(label))
    priv["repository_paths"][key] = paths.join(repo_root, label.package, label.name)
    priv["relative_paths"][key] = paths.join(label.package, label.name)
    if seed_root:
        _seed_root(priv, rctx_path, label)

################################################################################
def _add_sibling(priv, repo_root, sibling_key, key, path):
    if not _has(priv, sibling_key):
        fail("sibling_key not found '{}'".format(sibling_key))
    label = _make_sibling_label(_label(priv, sibling_key), path)
    priv["labels"][key] = label
    priv["paths"][key] = paths.normalize(paths.join(paths.dirname(_path(priv, sibling_key)), path))
    priv["repository_paths"][key] = paths.join(repo_root, label.package, label.name)
    priv["relative_paths"][key] = paths.join(label.package, label.name)

################################################################################
def _add_root(priv, repo_root, key, path):
    if not priv["root"]:
        fail("root paths can only be added after repository_label_store root is seeded with seed_root")
    root_workspace = priv["root"]["workspace"]
    root_path = priv["root"]["path"]

    # we have no idea what package this path is in so just assume the root package which works for repository rules
    label = Label("@{}//:{}".format(root_workspace, path))
    priv["labels"][key] = label
    priv["paths"][key] = paths.join(root_path, path)
    priv["repository_paths"][key] = paths.join(repo_root, path)
    priv["relative_paths"][key] = path

################################################################################
def _has(priv, key):
    return key in priv["labels"]

################################################################################
def _label(priv, key):
    return priv["labels"][key]

################################################################################
def _path(priv, key):
    return priv["paths"][key]

################################################################################
def _repository_path(priv, key):
    return priv["repository_paths"][key]

################################################################################
def _relative_path(priv, key):
    return priv["relative_paths"][key]

################################################################################
def _new(rctx_path):
    priv = {
        "root": None,
        "labels": {},
        "paths": {},
        "repository_paths": {},
        "relative_paths": {},
    }

    repo_root = str(rctx_path(""))

    return struct(
        repo_root = repo_root,
        seed_root = lambda label: _seed_root(priv, rctx_path, label),
        add = lambda key, label, seed_root = False: _add(priv, rctx_path, repo_root, key, label, seed_root),
        add_sibling = lambda sibling_key, key, path: _add_sibling(priv, repo_root, sibling_key, key, path),
        add_root = lambda key, path: _add_root(priv, repo_root, key, path),
        has = lambda key: _has(priv, key),
        label = lambda key: _label(priv, key),
        path = lambda key: _path(priv, key),
        repository_path = lambda key: _repository_path(priv, key),
        relative_path = lambda key: _relative_path(priv, key),
    )

repository_label_store = struct(
    new = _new,
)
