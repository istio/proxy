"""LLVM extension for use with bzlmod"""

load("@bazel_features//:features.bzl", "bazel_features")
load("@toolchains_llvm//toolchain:rules.bzl", "llvm_toolchain")
load(
    "@toolchains_llvm//toolchain/internal:repo.bzl",
    _llvm_config_attrs = "llvm_config_attrs",
    _llvm_repo_attrs = "llvm_repo_attrs",
)
load(
    "//toolchain/internal:common.bzl",
    _is_absolute_path = "is_absolute_path",
)

def _root_dict(roots, cls, name, strip_target):
    res = {}
    for root in roots:
        targets = list(root.targets)
        if not targets:
            targets = [""]
        for target in targets:
            if res.get(target):
                fail("duplicate target '%s' found for %s with name '%s'" % (target, cls, name))
            if bool(root.path) == (root.label):
                fail("target '%s' for %s with name '%s' must have either path or label, but not both" % (target, cls, name))
            if root.path:
                if not _is_absolute_path(root.path):
                    fail("target '%s' for %s with name '%s' must have an absolute path value" % (target, cls, name))
                res.update([(target, root.path)])
                continue
            label_str = str(root.label)
            if strip_target:
                label_str = label_str.split(":")[0]
            res.update([(target, label_str)])

    return res

def _constraint_dict(tags, name):
    constraints = {}

    # Gather all the additional constraints for each target
    for tag in tags:
        targets = list(tag.targets)
        if not targets:
            targets = [""]
        for target in targets:
            constraints_for_target = constraints.setdefault(target, [])
            constraints_for_target.extend([str(c) for c in tag.constraints])

    return constraints

def _llvm_impl_(module_ctx):
    for mod in module_ctx.modules:
        if not mod.is_root:
            fail("Only the root module can use the 'llvm' extension")
        toolchain_names = []
        for toolchain_attr in mod.tags.toolchain:
            name = toolchain_attr.name
            toolchain_names.append(name)
            attrs = {
                key: getattr(toolchain_attr, key)
                for key in dir(toolchain_attr)
                if not key.startswith("_")
            }
            attrs["toolchain_roots"] = _root_dict([root for root in mod.tags.toolchain_root if root.name == name], "toolchain_root", name, True)
            attrs["sysroot"] = _root_dict([sysroot for sysroot in mod.tags.sysroot if sysroot.name == name], "sysroot", name, False)
            attrs["extra_exec_compatible_with"] = _constraint_dict(
                [tag for tag in mod.tags.extra_exec_compatible_with if tag.name == name],
                name,
            )
            attrs["extra_target_compatible_with"] = _constraint_dict(
                [tag for tag in mod.tags.extra_target_compatible_with if tag.name == name],
                name,
            )

            cxx_cross_lib = {}
            for cl in mod.tags.cxx_cross_lib:
                if cl.name != name:
                    continue
                for target in cl.targets:
                    if cxx_cross_lib.get(target):
                        fail("duplicate target '%s' for cxx_cross_lib with name '%s'" % (target, name))
                    cxx_cross_lib[target] = str(cl.label)
            attrs["cxx_cross_lib"] = cxx_cross_lib

            llvm_toolchain(
                **attrs
            )

        # Check that every defined toolchain_root, sysroot, or cxx_cross_lib has a corresponding toolchain.
        for root in mod.tags.toolchain_root:
            if root.name not in toolchain_names:
                fail("toolchain_root '%s' does not have a corresponding toolchain" % root.name)
        for root in mod.tags.sysroot:
            if root.name not in toolchain_names:
                fail("sysroot '%s' does not have a corresponding toolchain" % root.name)
        for cl in mod.tags.cxx_cross_lib:
            if cl.name not in toolchain_names:
                fail("cxx_cross_lib '%s' does not have a corresponding toolchain" % cl.name)

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return module_ctx.extension_metadata(reproducible = True)
    else:
        return None

_attrs = {
    "name": attr.string(doc = """\
        Base name for the generated repositories, allowing more than one LLVM toolchain to be registered.
    """, default = "llvm_toolchain"),
}
_attrs.update(_llvm_config_attrs)
_attrs.update(_llvm_repo_attrs)

_attrs.pop("toolchain_roots", None)
_attrs.pop("sysroot", None)
_attrs.pop("cxx_cross_lib", None)

llvm = module_extension(
    implementation = _llvm_impl_,
    tag_classes = {
        "toolchain": tag_class(
            attrs = _attrs,
        ),
        "toolchain_root": tag_class(
            attrs = {
                "name": attr.string(doc = "Same name as the toolchain tag.", default = "llvm_toolchain"),
                "targets": attr.string_list(doc = "Specific targets, if any; empty list means this applies to all."),
                "label": attr.label(doc = "Dummy label whose package path is the toolchain root package."),
                "path": attr.string(doc = "Absolute path to the toolchain root."),
            },
        ),
        "sysroot": tag_class(
            attrs = {
                "name": attr.string(doc = "Same name as the toolchain tag.", default = "llvm_toolchain"),
                "targets": attr.string_list(doc = "Specific targets, if any; empty list means this applies to all."),
                "label": attr.label(doc = "Label containing the files with its package path as the sysroot path."),
                "path": attr.string(doc = "Absolute path to the sysroot."),
            },
        ),
        "extra_exec_compatible_with": tag_class(
            attrs = {
                "name": attr.string(doc = "Same name as the toolchain tag.", default = "llvm_toolchain"),
                "targets": attr.string_list(doc = "Specific targets, if any; empty list means this applies to all."),
                "constraints": attr.label_list(doc = "List of extra constraints to add to exec_compatible_with for the generated toolchains."),
            },
        ),
        "extra_target_compatible_with": tag_class(
            attrs = {
                "name": attr.string(doc = "Same name as the toolchain tag.", default = "llvm_toolchain"),
                "targets": attr.string_list(doc = "Specific targets, if any; empty list means this applies to all."),
                "constraints": attr.label_list(doc = "List of extra constraints to add to target_compatible_with for the generated toolchains."),
            },
        ),
        "cxx_cross_lib": tag_class(
            attrs = {
                "name": attr.string(doc = "Name of the toolchain this cxx_cross_lib applies to; must match a toolchain tag name.", default = "llvm_toolchain"),
                "targets": attr.string_list(doc = "Target pairs, e.g. linux-aarch64, linux-x86_64."),
                "label": attr.label(doc = "Label of a repo containing include/ and lib/ for the C++ standard library."),
            },
        ),
    },
)
