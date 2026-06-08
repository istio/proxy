"""Internal constants."""

# Access any of these globals via bazel_features.globals.<name>
# If the current version of Bazel doesn't have this global, it will be None.
# symbol -> (introduced_in, removed_in)
GLOBALS = {
    # + https://github.com/bazelbuild/bazel/commit/2aa06cf227fe349195191107286167035f0c5431
    # - https://github.com/bazelbuild/bazel/commit/86c8b0ff16e91775b0d42107cc4753f4f21d54fb
    "cc_proto_aspect": ("7.0.0-pre.20230405.2", "8.0.0"),
    # + https://github.com/bazelbuild/bazel/commit/c2d50de6ce2652e7fd56170663fd11e75098f35c
    # - https://github.com/bazelbuild/bazel/commit/71ca0ed111ff3d842a0d23bc3a46bd2e6745491d
    "CcSharedLibraryInfo": ("6.0.0-pre.20220630.1", "9.0.0-pre.20250921.2"),
    # + https://github.com/bazelbuild/bazel/commit/dbb09c9ea84cc6099ad7a30fa8206130d025f7ad
    # - https://github.com/bazelbuild/bazel/commit/0065fb2ff1ddf98483849181229e675b8820085b
    "CcSharedLibraryHintInfo": ("7.0.0-pre.20230316.2", "9.0.0"),
    "JavaInfo": ("", "8.0.0"),
    "JavaPluginInfo": ("", "8.0.0"),
    # https://github.com/bazelbuild/bazel/commit/0c100efeba05577b8bc334e1fae31149854bb0b7
    "macro": ("8.0.0", ""),
    # https://github.com/bazelbuild/bazel/commit/d1d35b280af1459458f996502e255d3774f391c2
    "PackageSpecificationInfo": ("6.4.0", ""),
    "ProtoInfo": ("", "8.0.0"),
    "PyCcLinkParamsProvider": ("", "8.0.0"),
    "PyInfo": ("", "8.0.0"),
    "PyRuntimeInfo": ("", "8.0.0"),
    # https://github.com/bazelbuild/bazel/pull/15232
    "RunEnvironmentInfo": ("5.3.0", ""),
    # https://github.com/bazelbuild/bazel/commit/8ad5f32889d687bce4de833d87e56e7ef19989d8
    # https://github.com/bazelbuild/bazel/commit/c5e08d4de65167e91045d99e89dc4b6a17e9fb39
    "set": ("8.1.0", ""),
    # https://github.com/bazelbuild/bazel/commit/e95c682bd1a1ab4495161b5a4423ad874112ad3f
    "subrule": ("7.0.0", ""),
    # Only used for testing bazel_features itself.
    "DefaultInfo": ("0.0.1", ""),
    "__TestingOnly_NeverAvailable": ("1000000000.0.0", ""),
}
